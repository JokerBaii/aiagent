/**
 * @file HttpsJsonClient.cpp
 * @brief OpenSSL HTTPS JSON POST 客户端实现。
 */

#include "cc/llm/HttpsJsonClient.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <memory>
#include <sstream>

namespace cc {
namespace {

struct SslContextDeleter {
    void operator()(SSL_CTX* context) const {
        SSL_CTX_free(context);
    }
};

struct BioDeleter {
    void operator()(BIO* bio) const {
        BIO_free_all(bio);
    }
};

[[nodiscard]] std::string sslErrorText() {
    const auto code = ERR_get_error();
    if (code == 0U) {
        return "未知 OpenSSL 错误";
    }
    char buffer[256];
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return buffer;
}

[[nodiscard]] std::string
buildRequest(const Endpoint& endpoint,
             const std::vector<std::pair<std::string, std::string>>& headers,
             const std::string& body) {
    std::ostringstream request;
    request << "POST " << endpoint.target << " HTTP/1.1\r\n"
            << "Host: " << endpoint.host << "\r\n"
            << "User-Agent: contest-compiler/0.1\r\n"
            << "Accept: application/json\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n";
    for (const auto& [name, value] : headers) {
        request << name << ": " << value << "\r\n";
    }
    request << "Connection: close\r\n\r\n" << body;
    return request.str();
}

} // namespace

Result<HttpResponse>
HttpsJsonClient::postJson(const Endpoint& endpoint,
                          const std::vector<std::pair<std::string, std::string>>& headers,
                          const std::string& body) const {
    // LLM 调用会把审计摘要发往外部 endpoint，因此本函数只由 LlmBrain 在显式授权后调用。
    std::unique_ptr<SSL_CTX, SslContextDeleter> context{SSL_CTX_new(TLS_client_method())};
    if (!context) {
        return Result<HttpResponse>::failure("初始化 OpenSSL TLS 上下文失败: " + sslErrorText());
    }
    if (SSL_CTX_set_default_verify_paths(context.get()) != 1) {
        return Result<HttpResponse>::failure("加载系统 CA 证书失败: " + sslErrorText());
    }
    SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER, nullptr);

    std::unique_ptr<BIO, BioDeleter> bio{BIO_new_ssl_connect(context.get())};
    if (!bio) {
        return Result<HttpResponse>::failure("创建 HTTPS 连接失败: " + sslErrorText());
    }

    SSL* ssl = nullptr;
    BIO_get_ssl(bio.get(), &ssl);
    if (ssl == nullptr) {
        return Result<HttpResponse>::failure("获取 TLS 句柄失败");
    }
    SSL_set_tlsext_host_name(ssl, endpoint.host.c_str());
    SSL_set1_host(ssl, endpoint.host.c_str());

    const auto connectTarget = endpoint.host + ":" + endpoint.port;
    BIO_set_conn_hostname(bio.get(), connectTarget.c_str());
    if (BIO_do_connect(bio.get()) <= 0) {
        return Result<HttpResponse>::failure("连接 LLM endpoint 失败: " + sslErrorText());
    }
    if (BIO_do_handshake(bio.get()) <= 0) {
        return Result<HttpResponse>::failure("LLM endpoint TLS 握手失败: " + sslErrorText());
    }

    const auto request = buildRequest(endpoint, headers, body);
    if (BIO_write(bio.get(), request.data(), static_cast<int>(request.size())) <= 0) {
        return Result<HttpResponse>::failure("写入 LLM HTTP 请求失败: " + sslErrorText());
    }

    std::string response;
    char buffer[4096];
    while (true) {
        const auto read = BIO_read(bio.get(), buffer, sizeof(buffer));
        if (read > 0) {
            response.append(buffer, static_cast<std::size_t>(read));
            continue;
        }
        if (BIO_should_retry(bio.get()) != 0) {
            continue;
        }
        break;
    }
    if (response.empty()) {
        return Result<HttpResponse>::failure("LLM endpoint 未返回 HTTP 响应");
    }
    return HttpResponseParser{}.parse(response);
}

} // namespace cc
