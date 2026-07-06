/**
 * @file HttpsJsonClient.hpp
 * @brief OpenSSL HTTPS JSON POST 客户端。
 */

#pragma once

#include "cc/core/Result.hpp"
#include "cc/llm/EndpointParser.hpp"
#include "cc/llm/HttpResponseParser.hpp"

#include <string>
#include <utility>
#include <vector>

namespace cc {

/**
 * @brief HTTPS JSON POST 客户端。
 *
 * 本类只被 LlmBrain 在显式授权后调用，负责 TLS 校验和 JSON 请求发送。
 */
class HttpsJsonClient {
  public:
    /**
     * @brief 向 HTTPS endpoint 发送 JSON POST。
     *
     * @return 成功时返回 HTTP 响应；失败时返回连接、TLS 或解析错误。
     */
    [[nodiscard]] Result<HttpResponse>
    postJson(const Endpoint& endpoint,
             const std::vector<std::pair<std::string, std::string>>& headers,
             const std::string& body) const;
};

} // namespace cc
