/**
 * @file HttpResponseParser.cpp
 * @brief 严格 HTTP/1.x 响应解析实现。
 */

#include "cc/llm/HttpResponseParser.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace cc {
namespace {

struct ChunkDecodeResult {
    bool complete{false};
    std::size_t consumed{0U};
    std::string decoded;
};

[[nodiscard]] std::string lowerHeader(std::string value) {
    return util::lowerAscii(util::trim(std::move(value)));
}

[[nodiscard]] bool validHeaderValue(std::string_view value) {
    return std::none_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return character == '\r' || character == '\n' || byte == 0U || byte == 127U ||
               (byte < 32U && character != '\t');
    });
}

[[nodiscard]] bool validHeaderName(std::string_view name) {
    constexpr std::string_view punctuation{"!#$%&'*+-.^_`|~"};
    return !name.empty() && std::all_of(name.begin(), name.end(), [&](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return (byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') ||
               (byte >= 'a' && byte <= 'z') ||
               punctuation.find(character) != std::string_view::npos;
    });
}

[[nodiscard]] Result<std::map<std::string, std::string>>
parseHeaders(const std::string& headerText) {
    std::istringstream headerStream{headerText};
    std::string line;
    std::getline(headerStream, line);

    std::map<std::string, std::string> headers;
    while (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.front() == ' ' || line.front() == '\t') {
            return Result<std::map<std::string, std::string>>::failure(
                "LLM HTTP 响应不允许折叠 header");
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos || colon == 0U) {
            return Result<std::map<std::string, std::string>>::failure(
                "LLM HTTP 响应 header 格式非法");
        }
        const auto rawName = std::string_view{line}.substr(0U, colon);
        if (!validHeaderName(rawName)) {
            return Result<std::map<std::string, std::string>>::failure(
                "LLM HTTP 响应 header 名称非法");
        }
        const auto name = lowerHeader(std::string{rawName});
        const auto value = util::trim(line.substr(colon + 1U));
        if (!validHeaderValue(value)) {
            return Result<std::map<std::string, std::string>>::failure(
                "LLM HTTP 响应 header 包含控制字符");
        }
        const auto existing = headers.find(name);
        if (existing != headers.end()) {
            if (name == "content-length" || name == "transfer-encoding") {
                return Result<std::map<std::string, std::string>>::failure(
                    "LLM HTTP 响应包含歧义长度 header");
            }
            existing->second += ", " + value;
        } else {
            headers.emplace(name, value);
        }
    }
    return Result<std::map<std::string, std::string>>::success(std::move(headers));
}

[[nodiscard]] Result<std::size_t> parseDecimalSize(std::string_view value) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](char character) {
            return character >= '0' && character <= '9';
        })) {
        return Result<std::size_t>::failure("LLM HTTP Content-Length 非法");
    }
    std::size_t result = 0U;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        return Result<std::size_t>::failure("LLM HTTP Content-Length 超出范围");
    }
    return Result<std::size_t>::success(result);
}

[[nodiscard]] Result<std::size_t> parseChunkSize(std::string_view line) {
    if (line.size() > 1024U || !validHeaderValue(line)) {
        return Result<std::size_t>::failure("HTTP chunked 响应块头非法");
    }
    const auto extension = line.find(';');
    const auto sizeText = util::trim(std::string{line.substr(0U, extension)});
    if (sizeText.empty() || sizeText.size() > sizeof(std::size_t) * 2U ||
        !std::all_of(sizeText.begin(), sizeText.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                   (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
        })) {
        return Result<std::size_t>::failure("HTTP chunked 响应块长度解析失败");
    }
    std::size_t size = 0U;
    const auto parsed =
        std::from_chars(sizeText.data(), sizeText.data() + sizeText.size(), size, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != sizeText.data() + sizeText.size()) {
        return Result<std::size_t>::failure("HTTP chunked 响应块长度超出范围");
    }
    return Result<std::size_t>::success(size);
}

[[nodiscard]] Result<ChunkDecodeResult> decodeChunked(const std::string& body) {
    ChunkDecodeResult result;
    std::size_t offset = 0U;
    while (true) {
        const auto lineEnd = body.find("\r\n", offset);
        if (lineEnd == std::string::npos) {
            return Result<ChunkDecodeResult>::success(std::move(result));
        }
        auto chunkSize = parseChunkSize(std::string_view{body}.substr(offset, lineEnd - offset));
        if (!chunkSize.ok()) {
            return Result<ChunkDecodeResult>::failure(chunkSize.error());
        }
        offset = lineEnd + 2U;
        if (chunkSize.value() == 0U) {
            while (true) {
                const auto trailerEnd = body.find("\r\n", offset);
                if (trailerEnd == std::string::npos) {
                    return Result<ChunkDecodeResult>::success(std::move(result));
                }
                if (trailerEnd == offset) {
                    result.complete = true;
                    result.consumed = trailerEnd + 2U;
                    return Result<ChunkDecodeResult>::success(std::move(result));
                }
                const auto trailer = std::string_view{body}.substr(offset, trailerEnd - offset);
                const auto colon = trailer.find(':');
                if (trailer.front() == ' ' || trailer.front() == '\t' ||
                    colon == std::string_view::npos ||
                    !validHeaderName(trailer.substr(0U, colon)) ||
                    !validHeaderValue(trailer.substr(colon + 1U))) {
                    return Result<ChunkDecodeResult>::failure("HTTP chunked 响应 trailer 非法");
                }
                offset = trailerEnd + 2U;
            }
        }
        if (chunkSize.value() > body.size() - offset) {
            return Result<ChunkDecodeResult>::success(std::move(result));
        }
        if (chunkSize.value() > std::numeric_limits<std::size_t>::max() - result.decoded.size()) {
            return Result<ChunkDecodeResult>::failure("HTTP chunked 响应解码长度溢出");
        }
        result.decoded.append(body, offset, chunkSize.value());
        offset += chunkSize.value();
        if (body.size() - offset < 2U) {
            return Result<ChunkDecodeResult>::success(std::move(result));
        }
        if (body.compare(offset, 2U, "\r\n") != 0) {
            return Result<ChunkDecodeResult>::failure("HTTP chunked 响应块缺少结束标记");
        }
        offset += 2U;
    }
}

[[nodiscard]] Result<int> parseStatusCode(const std::string& statusLine) {
    if (!validHeaderValue(statusLine)) {
        return Result<int>::failure("LLM HTTP 状态行包含控制字符");
    }
    if (!(statusLine.rfind("HTTP/1.1 ", 0U) == 0U || statusLine.rfind("HTTP/1.0 ", 0U) == 0U)) {
        return Result<int>::failure("LLM HTTP 状态行协议非法");
    }
    if (statusLine.size() < 12U) {
        return Result<int>::failure("LLM HTTP 状态行解析失败");
    }
    int code = 0;
    const auto parsed = std::from_chars(statusLine.data() + 9U, statusLine.data() + 12U, code);
    if (parsed.ec != std::errc{} || parsed.ptr != statusLine.data() + 12U || code < 100 ||
        code > 599 || (statusLine.size() > 12U && statusLine[12U] != ' ')) {
        return Result<int>::failure("LLM HTTP 状态行解析失败");
    }
    return Result<int>::success(code);
}

} // namespace

bool HttpResponseParser::isComplete(const std::string& response) const {
    const auto split = response.find("\r\n\r\n");
    if (split == std::string::npos) {
        return false;
    }
    auto headers = parseHeaders(response.substr(0U, split));
    if (!headers.ok()) {
        return false;
    }
    const auto& values = headers.value();
    const auto body = response.substr(split + 4U);
    const auto transfer = values.find("transfer-encoding");
    const auto length = values.find("content-length");
    if (transfer != values.end() && length != values.end()) {
        return false;
    }
    if (transfer != values.end()) {
        if (lowerHeader(transfer->second) != "chunked") {
            return false;
        }
        auto decoded = decodeChunked(body);
        return decoded.ok() && decoded.value().complete && decoded.value().consumed == body.size();
    }
    if (length == values.end()) {
        return false;
    }
    auto expected = parseDecimalSize(length->second);
    return expected.ok() && body.size() == expected.value();
}

Result<HttpResponse> HttpResponseParser::parse(const std::string& response) const {
    const auto split = response.find("\r\n\r\n");
    if (split == std::string::npos) {
        return Result<HttpResponse>::failure("LLM HTTP 响应缺少头部结束标记");
    }

    const auto statusEnd = response.find("\r\n");
    if (statusEnd == std::string::npos || statusEnd > split) {
        return Result<HttpResponse>::failure("LLM HTTP 状态行解析失败");
    }
    auto status = parseStatusCode(response.substr(0U, statusEnd));
    if (!status.ok()) {
        return Result<HttpResponse>::failure(status.error());
    }
    auto headers = parseHeaders(response.substr(0U, split));
    if (!headers.ok()) {
        return Result<HttpResponse>::failure(headers.error());
    }

    HttpResponse parsed;
    parsed.statusCode = status.value();
    parsed.headers = std::move(headers.value());
    parsed.body = response.substr(split + 4U);
    const auto transfer = parsed.headers.find("transfer-encoding");
    const auto length = parsed.headers.find("content-length");
    if (transfer != parsed.headers.end() && length != parsed.headers.end()) {
        return Result<HttpResponse>::failure(
            "LLM HTTP 响应同时包含 Transfer-Encoding 和 Content-Length");
    }
    if (transfer != parsed.headers.end()) {
        if (lowerHeader(transfer->second) != "chunked") {
            return Result<HttpResponse>::failure("LLM HTTP 响应使用了不支持的传输编码");
        }
        auto decoded = decodeChunked(parsed.body);
        if (!decoded.ok()) {
            return Result<HttpResponse>::failure(decoded.error());
        }
        if (!decoded.value().complete || decoded.value().consumed != parsed.body.size()) {
            return Result<HttpResponse>::failure("HTTP chunked 响应不完整或包含尾随数据");
        }
        parsed.body = std::move(decoded.value().decoded);
    } else if (length != parsed.headers.end()) {
        auto expected = parseDecimalSize(length->second);
        if (!expected.ok()) {
            return Result<HttpResponse>::failure(expected.error());
        }
        if (parsed.body.size() != expected.value()) {
            return Result<HttpResponse>::failure("LLM HTTP 响应长度与 Content-Length 不一致");
        }
    }
    return Result<HttpResponse>::success(std::move(parsed));
}

} // namespace cc
