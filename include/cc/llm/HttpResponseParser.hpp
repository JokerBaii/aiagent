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
     * @brief 判断缓冲区是否已包含一份完整的 HTTP/1.1 响应。
     *
     * 支持 Content-Length 和 chunked 边界；没有显式长度时由连接关闭表示完成。
     */
    [[nodiscard]] bool isComplete(const std::string& response) const;

    /**
     * @brief 解析原始 HTTP 响应文本。
     */
    [[nodiscard]] Result<HttpResponse> parse(const std::string& response) const;
};

} // namespace cc
