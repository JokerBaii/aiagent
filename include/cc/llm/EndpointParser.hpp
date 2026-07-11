/**
 * @file EndpointParser.hpp
 * @brief HTTPS endpoint 解析。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <string>

namespace cc {

/**
 * @brief HTTPS endpoint 解析结果。
 */
struct Endpoint {
    std::string host;
    std::string port;
    std::string target;
    std::string hostHeader;
};

/**
 * @brief OpenAI-compatible HTTPS endpoint 解析器。
 *
 * 只接受 https URL，避免把 API key 发送到明文 HTTP endpoint。
 */
class EndpointParser {
  public:
    /**
     * @brief 解析 endpoint 字符串。
     */
    [[nodiscard]] Result<Endpoint> parse(std::string endpoint) const;
};

} // namespace cc
