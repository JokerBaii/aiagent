/**
 * @file HttpsJsonClient.cpp
 * @brief 有界 OpenSSL HTTPS JSON POST 客户端。
 */

#include "cc/llm/HttpsJsonClient.hpp"
#include "cc/util/StringUtil.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <poll.h>
#include <set>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

namespace cc {
namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaximumHeaderCount = 32U;
constexpr std::size_t kMaximumHeaderNameBytes = 128U;
constexpr std::size_t kMaximumHeaderValueBytes = 8192U;
constexpr std::size_t kAbsoluteMaximumRequestBodyBytes = 8U * 1024U * 1024U;
constexpr std::size_t kAbsoluteMaximumResponseBytes = 64U * 1024U * 1024U;
constexpr std::size_t kAbsoluteMaximumResponseHeaderBytes = 256U * 1024U;
constexpr auto kCancellationPollInterval = std::chrono::milliseconds{100};
constexpr auto kMaximumPhaseTimeout = std::chrono::minutes{10};
constexpr auto kMaximumTotalTimeout = std::chrono::minutes{15};

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
    return found ? output.str() : "未知 OpenSSL 错误";
}

[[nodiscard]] bool cancelled(const HttpsRequestOptions& options) {
    if (!options.isCancelled) {
        return false;
    }
    try {
        return options.isCancelled();
    } catch (...) {
        return true;
    }
}

[[nodiscard]] bool validHeaderName(std::string_view name) {
    if (name.empty() || name.size() > kMaximumHeaderNameBytes) {
        return false;
    }
    constexpr std::string_view punctuation{"!#$%&'*+-.^_`|~"};
    for (const auto character : name) {
        const auto byte = static_cast<unsigned char>(character);
        if (!((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') ||
              (byte >= 'a' && byte <= 'z') || punctuation.find(character) != std::string::npos)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool validHeaderValue(std::string_view value) {
    if (value.size() > kMaximumHeaderValueBytes) {
        return false;
    }
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '\r' || character == '\n' || byte == 0U || byte == 127U ||
            (byte < 32U && character != '\t')) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool reservedHeader(std::string name) {
    name = util::lowerAscii(std::move(name));
    return name == "host" || name == "content-length" || name == "content-type" ||
           name == "transfer-encoding" || name == "connection" || name == "user-agent" ||
           name == "accept";
}

[[nodiscard]] Result<void> validateOptions(const HttpsRequestOptions& options) {
    if (options.connectTimeout.count() <= 0 || options.writeTimeout.count() <= 0 ||
        options.readTimeout.count() <= 0 || options.totalTimeout.count() <= 0) {
        return Result<void>::failure("HTTPS 请求超时必须大于 0");
    }
    if (options.connectTimeout > kMaximumPhaseTimeout ||
        options.writeTimeout > kMaximumPhaseTimeout || options.readTimeout > kMaximumPhaseTimeout ||
        options.totalTimeout > kMaximumTotalTimeout) {
        return Result<void>::failure("HTTPS 请求超时超过允许上限");
    }
    if (options.maxRequestBodyBytes == 0U || options.maxResponseBytes == 0U ||
        options.maxResponseHeaderBytes == 0U ||
        options.maxResponseHeaderBytes > options.maxResponseBytes ||
        options.maxRequestBodyBytes > kAbsoluteMaximumRequestBodyBytes ||
        options.maxResponseBytes > kAbsoluteMaximumResponseBytes ||
        options.maxResponseHeaderBytes > kAbsoluteMaximumResponseHeaderBytes) {
        return Result<void>::failure("HTTPS 请求资源上限配置非法");
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void>
validateRequest(const Endpoint& endpoint,
                const std::vector<std::pair<std::string, std::string>>& headers,
                const std::string& body, const HttpsRequestOptions& options) {
    auto optionsValid = validateOptions(options);
    if (!optionsValid.ok()) {
        return optionsValid;
    }
    if (cancelled(options)) {
        return Result<void>::failure("HTTPS 请求已取消");
    }
    const auto canonical =
        EndpointParser{}.parse("https://" + endpoint.hostHeader + endpoint.target);
    if (!canonical.ok() || canonical.value().host != endpoint.host ||
        canonical.value().port != endpoint.port || canonical.value().target != endpoint.target ||
        canonical.value().hostHeader != endpoint.hostHeader) {
        return Result<void>::failure("HTTPS endpoint 未经安全解析或包含非法字符");
    }
    if (body.size() > options.maxRequestBodyBytes) {
        return Result<void>::failure("LLM 请求体超过允许上限");
    }
    if (headers.size() > kMaximumHeaderCount) {
        return Result<void>::failure("LLM HTTP header 数量超过允许上限");
    }
    std::set<std::string> headerNames;
    for (const auto& [name, value] : headers) {
        if (!validHeaderName(name) || !validHeaderValue(value)) {
            return Result<void>::failure("LLM HTTP header 名称或值非法");
        }
        if (reservedHeader(name)) {
            return Result<void>::failure("LLM HTTP header 不允许覆盖协议保留字段");
        }
        if (!headerNames.insert(util::lowerAscii(name)).second) {
            return Result<void>::failure("LLM HTTP header 名称重复");
        }
    }
    return Result<void>::success();
}

[[nodiscard]] std::string
buildRequest(const Endpoint& endpoint,
             const std::vector<std::pair<std::string, std::string>>& headers,
             const std::string& body) {
    std::ostringstream request;
    request << "POST " << endpoint.target << " HTTP/1.1\r\n"
            << "Host: " << endpoint.hostHeader << "\r\n"
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

[[nodiscard]] Clock::time_point earlier(Clock::time_point left, Clock::time_point right) {
    return left < right ? left : right;
}

[[nodiscard]] Result<void> waitForBio(BIO* bio, Clock::time_point deadline,
                                      const HttpsRequestOptions& options, std::string_view phase) {
    while (true) {
        if (cancelled(options)) {
            return Result<void>::failure("HTTPS 请求已取消");
        }
        const auto now = Clock::now();
        if (now >= deadline) {
            return Result<void>::failure(std::string{phase} + " LLM endpoint 超时");
        }

        int socket = -1;
        BIO_get_fd(bio, &socket);
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const auto waitFor =
            remaining < kCancellationPollInterval ? remaining : kCancellationPollInterval;
        if (socket < 0) {
            std::this_thread::sleep_for(waitFor);
            continue;
        }

        short events = 0;
        if (BIO_should_read(bio) != 0) {
            events = static_cast<short>(events | POLLIN);
        }
        if (BIO_should_write(bio) != 0) {
            events = static_cast<short>(events | POLLOUT);
        }
        if (events == 0) {
            events = static_cast<short>(POLLIN | POLLOUT);
        }
        pollfd descriptor{.fd = socket, .events = events, .revents = 0};
        const auto timeout = static_cast<int>(std::max<std::int64_t>(1, waitFor.count()));
        const auto ready = ::poll(&descriptor, 1, timeout);
        if (ready > 0) {
            if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
                return Result<void>::failure(std::string{phase} + " LLM endpoint 连接异常");
            }
            return Result<void>::success();
        }
        if (ready < 0 && errno != EINTR) {
            return Result<void>::failure(std::string{phase} + " LLM endpoint 等待失败");
        }
    }
}

[[nodiscard]] bool isIpAddress(const std::string& host) {
    in_addr ipv4{};
    in6_addr ipv6{};
    return inet_pton(AF_INET, host.c_str(), &ipv4) == 1 ||
           inet_pton(AF_INET6, host.c_str(), &ipv6) == 1;
}

[[nodiscard]] Result<std::unique_ptr<BIO, BioDeleter>>
connectTls(SSL_CTX* context, const Endpoint& endpoint, const HttpsRequestOptions& options,
           Clock::time_point totalDeadline) {
    ERR_clear_error();
    std::unique_ptr<BIO, BioDeleter> bio{BIO_new_ssl_connect(context)};
    if (!bio) {
        return Result<std::unique_ptr<BIO, BioDeleter>>::failure("创建 HTTPS 连接失败: " +
                                                                 sslErrorText());
    }

    SSL* ssl = nullptr;
    BIO_get_ssl(bio.get(), &ssl);
    if (ssl == nullptr) {
        return Result<std::unique_ptr<BIO, BioDeleter>>::failure("获取 TLS 句柄失败");
    }
    const auto ipAddress = isIpAddress(endpoint.host);
    auto* verifyParameters = SSL_get0_param(ssl);
    const auto hostConfigured =
        ipAddress ? X509_VERIFY_PARAM_set1_ip_asc(verifyParameters, endpoint.host.c_str())
                  : SSL_set1_host(ssl, endpoint.host.c_str());
    if (hostConfigured != 1 ||
        (!ipAddress && SSL_set_tlsext_host_name(ssl, endpoint.host.c_str()) != 1)) {
        return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
            "配置 LLM endpoint TLS 主机校验失败: " + sslErrorText());
    }
    static constexpr unsigned char alpnHttp11[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    if (SSL_set_alpn_protos(ssl, alpnHttp11, sizeof(alpnHttp11)) != 0) {
        return Result<std::unique_ptr<BIO, BioDeleter>>::failure("配置 LLM endpoint HTTP 协议失败");
    }

    const auto connectTarget =
        (endpoint.host.find(':') == std::string::npos ? endpoint.host : "[" + endpoint.host + "]") +
        ":" + endpoint.port;
    if (BIO_set_conn_hostname(bio.get(), connectTarget.c_str()) <= 0 ||
        BIO_set_nbio(bio.get(), 1) <= 0) {
        return Result<std::unique_ptr<BIO, BioDeleter>>::failure("配置 LLM endpoint 连接失败: " +
                                                                 sslErrorText());
    }
    const auto deadline = earlier(Clock::now() + options.connectTimeout, totalDeadline);
    while (true) {
        ERR_clear_error();
        const auto connected = BIO_do_connect(bio.get());
        if (connected == 1) {
            const auto verifyResult = SSL_get_verify_result(ssl);
            if (verifyResult != X509_V_OK) {
                return Result<std::unique_ptr<BIO, BioDeleter>>::failure(
                    "LLM endpoint TLS 证书校验失败: " +
                    std::string{X509_verify_cert_error_string(verifyResult)});
            }
            return Result<std::unique_ptr<BIO, BioDeleter>>::success(std::move(bio));
        }
        if (BIO_should_retry(bio.get()) == 0) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure("连接 LLM endpoint 失败: " +
                                                                     sslErrorText());
        }
        auto waited = waitForBio(bio.get(), deadline, options, "连接");
        if (!waited.ok()) {
            return Result<std::unique_ptr<BIO, BioDeleter>>::failure(waited.error());
        }
    }
}

[[nodiscard]] Result<void> writeAll(BIO* bio, const std::string& request,
                                    const HttpsRequestOptions& options,
                                    Clock::time_point totalDeadline) {
    std::size_t offset = 0U;
    auto deadline = earlier(Clock::now() + options.writeTimeout, totalDeadline);
    while (offset < request.size()) {
        if (cancelled(options)) {
            return Result<void>::failure("HTTPS 请求已取消");
        }
        ERR_clear_error();
        const auto remaining = request.size() - offset;
        const auto chunkSize = std::min<std::size_t>(
            remaining, static_cast<std::size_t>(std::numeric_limits<int>::max()));
        const auto written = BIO_write(bio, request.data() + offset, static_cast<int>(chunkSize));
        if (written > 0) {
            offset += static_cast<std::size_t>(written);
            continue;
        }
        if (BIO_should_retry(bio) == 0) {
            return Result<void>::failure("写入 LLM HTTP 请求失败: " + sslErrorText());
        }
        auto waited = waitForBio(bio, deadline, options, "写入");
        if (!waited.ok()) {
            return waited;
        }
    }
    return Result<void>::success();
}

[[nodiscard]] Result<std::size_t> declaredContentLength(const std::string& response) {
    const auto split = response.find("\r\n\r\n");
    if (split == std::string::npos) {
        return Result<std::size_t>::success(0U);
    }
    std::istringstream lines{response.substr(0U, split)};
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos ||
            util::lowerAscii(util::trim(line.substr(0U, colon))) != "content-length") {
            continue;
        }
        const auto value = util::trim(line.substr(colon + 1U));
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            })) {
            return Result<std::size_t>::failure("LLM HTTP Content-Length 非法");
        }
        std::size_t length = 0U;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
            return Result<std::size_t>::failure("LLM HTTP Content-Length 超出范围");
        }
        return Result<std::size_t>::success(length);
    }
    return Result<std::size_t>::success(0U);
}

[[nodiscard]] Result<std::string> readResponse(BIO* bio, const HttpsRequestOptions& options,
                                               Clock::time_point totalDeadline) {
    std::string response;
    response.reserve(std::min<std::size_t>(options.maxResponseBytes, 64U * 1024U));
    char buffer[16U * 1024U];
    auto idleDeadline = earlier(Clock::now() + options.readTimeout, totalDeadline);
    while (true) {
        if (cancelled(options)) {
            return Result<std::string>::failure("HTTPS 请求已取消");
        }
        ERR_clear_error();
        const auto read = BIO_read(bio, buffer, sizeof(buffer));
        if (read > 0) {
            const auto count = static_cast<std::size_t>(read);
            if (count > options.maxResponseBytes - response.size()) {
                return Result<std::string>::failure("LLM HTTP 响应超过允许上限");
            }
            response.append(buffer, count);
            const auto split = response.find("\r\n\r\n");
            if (split == std::string::npos && response.size() > options.maxResponseHeaderBytes) {
                return Result<std::string>::failure("LLM HTTP 响应头超过允许上限");
            }
            if (split != std::string::npos && split > options.maxResponseHeaderBytes) {
                return Result<std::string>::failure("LLM HTTP 响应头超过允许上限");
            }
            auto length = declaredContentLength(response);
            if (!length.ok()) {
                return Result<std::string>::failure(length.error());
            }
            if (split != std::string::npos &&
                length.value() > options.maxResponseBytes - (split + 4U)) {
                return Result<std::string>::failure("LLM HTTP 响应声明长度超过允许上限");
            }
            if (HttpResponseParser{}.isComplete(response)) {
                return Result<std::string>::success(std::move(response));
            }
            idleDeadline = earlier(Clock::now() + options.readTimeout, totalDeadline);
            continue;
        }
        if (BIO_should_retry(bio) != 0) {
            auto waited = waitForBio(bio, idleDeadline, options, "读取");
            if (!waited.ok()) {
                return Result<std::string>::failure(waited.error());
            }
            continue;
        }
        if (read < 0 && response.empty()) {
            return Result<std::string>::failure("读取 LLM HTTP 响应失败: " + sslErrorText());
        }
        return Result<std::string>::success(std::move(response));
    }
}

} // namespace

Result<HttpResponse>
HttpsJsonClient::postJson(const Endpoint& endpoint,
                          const std::vector<std::pair<std::string, std::string>>& headers,
                          const std::string& body, const HttpsRequestOptions& options) const {
    auto valid = validateRequest(endpoint, headers, body, options);
    if (!valid.ok()) {
        return Result<HttpResponse>::failure(valid.error());
    }
    const auto totalDeadline = Clock::now() + options.totalTimeout;

    std::unique_ptr<SSL_CTX, SslContextDeleter> context{SSL_CTX_new(TLS_client_method())};
    if (!context) {
        return Result<HttpResponse>::failure("初始化 OpenSSL TLS 上下文失败: " + sslErrorText());
    }
    if (SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_default_verify_paths(context.get()) != 1) {
        return Result<HttpResponse>::failure("加载 TLS 安全配置失败: " + sslErrorText());
    }
    SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER, nullptr);

    auto connected = connectTls(context.get(), endpoint, options, totalDeadline);
    if (!connected.ok()) {
        return Result<HttpResponse>::failure(connected.error());
    }
    auto bio = std::move(connected.value());

    auto written =
        writeAll(bio.get(), buildRequest(endpoint, headers, body), options, totalDeadline);
    if (!written.ok()) {
        return Result<HttpResponse>::failure(written.error());
    }
    auto response = readResponse(bio.get(), options, totalDeadline);
    if (!response.ok()) {
        return Result<HttpResponse>::failure(response.error());
    }
    if (response.value().empty()) {
        return Result<HttpResponse>::failure("LLM endpoint 未返回 HTTP 响应");
    }
    return HttpResponseParser{}.parse(response.value());
}

} // namespace cc
