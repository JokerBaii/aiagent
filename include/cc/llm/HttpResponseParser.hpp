/**
 * @file HttpResponseParser.hpp
 * @brief HTTP 响应解析。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <map>
#include <string>

namespace cc {

/**
 * @brief HTTP 响应结构。
 */
struct HttpResponse {
    int statusCode{0};
    std::map<std::string, std::string> headers;
    std::string body;
};

/**
 * @brief HTTP/1.1 响应解析器。
 *
 * 解析器支持 Content-Length 和 chunked 响应，用于 LLM HTTPS 客户端的最小可信闭环。
 */
class HttpResponseParser {
  public:
    /**
     * @brief 解析原始 HTTP 响应文本。
     */
    [[nodiscard]] Result<HttpResponse> parse(const std::string& response) const;
};

} // namespace cc
