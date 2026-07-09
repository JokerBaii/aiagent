/**
 * @file HttpsJsonClient.cpp
 * @brief OpenSSL HTTPS JSON POST 客户端实现。
 */

#include "cc/llm/HttpsJsonClient.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <sstream>
#include <thread>

namespace cc {
namespace {

constexpr int connectTimeoutSeconds = 12;
constexpr int connectAttemptCount = 3;
constexpr int connectRetryPollMilliseconds = 50;

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
    std::ostringstream output;
    bool found = false;
    while (const auto code = ERR_get_error()) {
        char buffer[256];
        ERR_error_string_n(code, buffer, sizeof(buffer));
        if (found) {
            output << "; ";
        }
        output << buffer;
        found = true;
    }
    if (!found) {
        return "未知 OpenSSL 错误";
    }
    return output.str();
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

[[nodiscard]] Result<std::unique_ptr<BIO, BioDeleter>>
connectTls(SSL_CTX* context, const Endpoint& endpoint) {
    std::string lastError = "未知连接错误";
    const auto connectTarget = endpoint.host + ":" + endpoint.port;

    for (int attempt = 1; attempt <= connectAttemptCount; ++attempt) {
        ERR_clear_error();
        std::unique_ptr<BIO, BioDeleter> bio{BIO_new_ssl_connect(context)};
        if (!bio) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
                "创建 HTTPS 连接失败: " + sslErrorText());
        }

        SSL* ssl = nullptr;
        BIO_get_ssl(bio.get(), &ssl);
        if (ssl == nullptr) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure("获取 TLS 句柄失败");
        }
        if (SSL_set_tlsext_host_name(ssl, endpoint.host.c_str()) != 1 ||
            SSL_set1_host(ssl, endpoint.host.c_str()) != 1) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
                "配置 LLM endpoint TLS 主机校验失败: " + sslErrorText());
        }
        static constexpr unsigned char alpnHttp11[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
        if (SSL_set_alpn_protos(ssl, alpnHttp11, sizeof(alpnHttp11)) != 0) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
                "配置 LLM endpoint HTTP 协议失败");
        }
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

        BIO_set_conn_hostname(bio.get(), connectTarget.c_str());
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        BIO_set_nbio(bio.get(), 1);
        const auto connected =
            BIO_do_connect_retry(bio.get(), connectTimeoutSeconds, connectRetryPollMilliseconds);
#else
        const auto connected = BIO_do_connect(bio.get());
#endif
        if (connected == 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            BIO_set_nbio(bio.get(), 0);
#endif
            return Result<std::unique_ptr<BIO, BioDeleter>>::success(std::move(bio));
        }

        const auto verifyResult = SSL_get_verify_result(ssl);
        if (verifyResult != X509_V_OK) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
                "LLM endpoint TLS 证书校验失败: " +
                std::string{X509_verify_cert_error_string(verifyResult)});
        }
        lastError = connected == 0 ? "连接超时" : sslErrorText();
        if (attempt < connectAttemptCount) {
            std::this_thread::sleep_for(std::chrono::milliseconds{150 * attempt});
        }
    }

    return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
        "连接 LLM endpoint 失败（已重试 " + std::to_string(connectAttemptCount) +
        " 次）: " + lastError +
        "。请检查网络、代理/TUN 状态及 endpoint 配置后重试");
}

[[nodiscard]] Result<bool> writeAll(BIO* bio, const std::string& request) {
    std::size_t offset = 0U;
    while (offset < request.size()) {
        ERR_clear_error();
        const auto remaining = request.size() - offset;
        const auto chunkSize = std::min<std::size_t>(remaining, 16U * 1024U);
        const auto written =
            BIO_write(bio, request.data() + offset, static_cast<int>(chunkSize));
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
            continue;
        }
        if (BIO_should_retry(bio) != 0) {
            continue;
        }
        return Result<bool>::failure("写入 LLM HTTP 请求失败: " + sslErrorText());
    }
    return Result<bool>::success(true);
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

    auto connected = connectTls(context.get(), endpoint);
    if (!connected.ok()) {
        return Result<HttpResponse>::failure(connected.error());
    }
    auto bio = std::move(connected.value());

    const auto request = buildRequest(endpoint, headers, body);
    auto written = writeAll(bio.get(), request);
    if (!written.ok()) {
        return Result<HttpResponse>::failure(written.error());
    }

    std::string response;
    char buffer[4096];
    while (true) {
        const auto read = BIO_read(bio.get(), buffer, sizeof(buffer));
        if (read > 0) {
            response.append(buffer, static_cast<std::size_t>(read));
            if (HttpResponseParser{}.isComplete(response)) {
                break;
            }
            continue;
        }
        if (BIO_should_retry(bio.get()) != 0) {
            continue;
        }
        if (read < 0 && response.empty()) {
            return Result<HttpResponse>::failure("读取 LLM HTTP 响应失败: " + sslErrorText());
        }
        break;
    }
    if (response.empty()) {
        return Result<HttpResponse>::failure("LLM endpoint 未返回 HTTP 响应");
    }
    return HttpResponseParser{}.parse(response);
}

} // namespace cc
