/**
 * @file HttpResponseParser.cpp
 * @brief HTTP 响应解析实现。
 */

#include "cc/llm/HttpResponseParser.hpp"
#include "cc/util/StringUtil.hpp"

#include <sstream>
#include <utility>

namespace cc {
namespace {

[[nodiscard]] std::string lowerHeader(std::string value) {
    return util::lowerAscii(util::trim(std::move(value)));
}

[[nodiscard]] Result<std::string> decodeChunked(const std::string& body) {
    std::string decoded;
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto lineEnd = body.find("\r\n", offset);
        if (lineEnd == std::string::npos) {
            return Result<std::string>::failure("HTTP chunked 响应不完整");
        }
        const auto sizeText = body.substr(offset, lineEnd - offset);
        std::size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(sizeText, nullptr, 16);
        } catch (const std::exception&) {
            return Result<std::string>::failure("HTTP chunked 响应块长度解析失败");
        }
        offset = lineEnd + 2U;
        if (chunkSize == 0U) {
            return Result<std::string>::success(decoded);
        }
        if (offset + chunkSize > body.size()) {
            return Result<std::string>::failure("HTTP chunked 响应长度不匹配");
        }
        decoded.append(body.substr(offset, chunkSize));
        offset += chunkSize + 2U;
    }
    return Result<std::string>::failure("HTTP chunked 响应缺少结束块");
}

} // namespace

Result<HttpResponse> HttpResponseParser::parse(const std::string& response) const {
    const auto split = response.find("\r\n\r\n");
    if (split == std::string::npos) {
        return Result<HttpResponse>::failure("LLM HTTP 响应缺少头部结束标记");
    }

    std::istringstream headerStream{response.substr(0, split)};
    std::string statusLine;
    std::getline(headerStream, statusLine);
    std::istringstream statusStream{statusLine};
    std::string httpVersion;
    HttpResponse parsed;
    statusStream >> httpVersion >> parsed.statusCode;
    if (parsed.statusCode == 0) {
        return Result<HttpResponse>::failure("LLM HTTP 状态行解析失败");
    }

    std::string line;
    while (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        parsed.headers[lowerHeader(line.substr(0, colon))] = util::trim(line.substr(colon + 1U));
    }

    parsed.body = response.substr(split + 4U);
    const auto transfer = parsed.headers.find("transfer-encoding");
    if (transfer != parsed.headers.end() && util::containsLower(transfer->second, "chunked")) {
        auto decoded = decodeChunked(parsed.body);
        if (!decoded.ok()) {
            return Result<HttpResponse>::failure(decoded.error());
        }
        parsed.body = std::move(decoded.value());
    }
    return Result<HttpResponse>::success(parsed);
}

} // namespace cc
