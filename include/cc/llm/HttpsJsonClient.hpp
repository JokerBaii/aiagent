/**
 * @file HttpsJsonClient.hpp
 * @brief OpenSSL HTTPS JSON GET/POST 客户端。
 */

#pragma once

#include "cc/core/Result.hpp"
#include "cc/llm/EndpointParser.hpp"
#include "cc/llm/HttpResponseParser.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cc {

/**
 * @brief 单次 HTTPS 请求的资源、时限和取消边界。
 */
struct HttpsRequestOptions {
    std::chrono::milliseconds connectTimeout{12000};
    std::chrono::milliseconds writeTimeout{10000};
    std::chrono::milliseconds readTimeout{60000};
    std::chrono::milliseconds totalTimeout{90000};
    std::size_t maxRequestBodyBytes{std::size_t{8U} * 1024U * 1024U};
    std::size_t maxResponseBytes{std::size_t{16U} * 1024U * 1024U};
    std::size_t maxResponseHeaderBytes{std::size_t{64U} * 1024U};
    std::function<bool()> isCancelled;
};

/**
 * @brief HTTPS JSON GET/POST 客户端。
 *
 * 本类只被 LlmBrain 在任务能力快照允许时调用，负责 TLS 校验和 JSON 请求发送。
 */
class HttpsJsonClient {
  public:
    /** @brief 发送 HTTPS GET 并解析 HTTP 响应，用于只读 provider 元数据。 */
    [[nodiscard]] Result<HttpResponse>
    getJson(const Endpoint& endpoint,
            const std::vector<std::pair<std::string, std::string>>& headers,
            const HttpsRequestOptions& options = {}) const;

    /**
     * @brief 向 HTTPS endpoint 发送 JSON POST。
     *
     * @return 成功时返回 HTTP 响应；失败时返回连接、TLS 或解析错误。
     */
    [[nodiscard]] Result<HttpResponse>
    postJson(const Endpoint& endpoint,
             const std::vector<std::pair<std::string, std::string>>& headers,
             const std::string& body, const HttpsRequestOptions& options = {}) const;
};

} // namespace cc
