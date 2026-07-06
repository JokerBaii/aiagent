/**
 * @file EndpointParser.cpp
 * @brief HTTPS endpoint 解析实现。
 */

#include "cc/llm/EndpointParser.hpp"

#include <string_view>

namespace cc {

Result<Endpoint> EndpointParser::parse(std::string endpoint) const {
    constexpr std::string_view prefix{"https://"};
    if (endpoint.rfind(prefix, 0) != 0U) {
        return Result<Endpoint>::failure("LLM endpoint 必须使用 https://");
    }
    endpoint.erase(0, prefix.size());
    const auto slash = endpoint.find('/');
    const auto authority = slash == std::string::npos ? endpoint : endpoint.substr(0, slash);
    Endpoint parsed;
    parsed.target = slash == std::string::npos ? "/" : endpoint.substr(slash);
    if (parsed.target.empty()) {
        parsed.target = "/";
    }
    const auto colon = authority.rfind(':');
    if (colon == std::string::npos) {
        parsed.host = authority;
        parsed.port = "443";
    } else {
        parsed.host = authority.substr(0, colon);
        parsed.port = authority.substr(colon + 1U);
    }
    if (parsed.host.empty() || parsed.port.empty()) {
        return Result<Endpoint>::failure("LLM endpoint 缺少主机或端口");
    }
    return Result<Endpoint>::success(parsed);
}

} // namespace cc
