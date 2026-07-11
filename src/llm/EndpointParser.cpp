/**
 * @file EndpointParser.cpp
 * @brief HTTPS endpoint 解析实现。
 */

#include "cc/llm/EndpointParser.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string_view>

namespace cc {
namespace {

constexpr std::size_t kMaximumEndpointBytes = 8192U;
constexpr std::size_t kMaximumHostBytes = 253U;

[[nodiscard]] bool hasControlOrWhitespace(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return std::iscntrl(byte) != 0 || std::isspace(byte) != 0;
    });
}

[[nodiscard]] bool validDnsHost(std::string_view host) {
    if (host.empty() || host.size() > kMaximumHostBytes || host.front() == '.' ||
        host.back() == '.') {
        return false;
    }
    if (!std::all_of(host.begin(), host.end(), [](char character) {
            const auto byte = static_cast<unsigned char>(character);
            return std::isalnum(byte) != 0 || character == '.' || character == '-';
        })) {
        return false;
    }
    std::size_t begin = 0U;
    while (begin < host.size()) {
        const auto end = host.find('.', begin);
        const auto length = (end == std::string_view::npos ? host.size() : end) - begin;
        if (length == 0U || length > 63U || host[begin] == '-' ||
            host[begin + length - 1U] == '-') {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return true;
}

[[nodiscard]] Result<std::string> parsePort(std::string_view text) {
    if (text.empty() || text.size() > 5U || !std::all_of(text.begin(), text.end(), [](char value) {
            return value >= '0' && value <= '9';
        })) {
        return Result<std::string>::failure("LLM endpoint 端口必须是 1 到 65535 的数字");
    }
    unsigned int port = 0U;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || port == 0U ||
        port > 65535U) {
        return Result<std::string>::failure("LLM endpoint 端口必须是 1 到 65535 的数字");
    }
    return Result<std::string>::success(std::to_string(port));
}

} // namespace

Result<Endpoint> EndpointParser::parse(std::string endpoint) const {
    constexpr std::string_view prefix{"https://"};
    if (endpoint.empty() || endpoint.size() > kMaximumEndpointBytes ||
        hasControlOrWhitespace(endpoint)) {
        return Result<Endpoint>::failure("LLM endpoint 为空、过长或包含空白/控制字符");
    }
    if (endpoint.rfind(prefix, 0) != 0U) {
        return Result<Endpoint>::failure("LLM endpoint 必须使用 https://");
    }
    endpoint.erase(0, prefix.size());
    if (endpoint.find('#') != std::string::npos) {
        return Result<Endpoint>::failure("LLM endpoint 不允许包含 URL fragment");
    }
    const auto targetBegin = endpoint.find_first_of("/?");
    const auto authority =
        targetBegin == std::string::npos ? endpoint : endpoint.substr(0, targetBegin);
    if (authority.empty() || authority.find('@') != std::string::npos) {
        return Result<Endpoint>::failure("LLM endpoint 缺少主机或包含不允许的 userinfo");
    }

    Endpoint parsed;
    if (targetBegin == std::string::npos) {
        parsed.target = "/";
    } else if (endpoint[targetBegin] == '?') {
        parsed.target = "/" + endpoint.substr(targetBegin);
    } else {
        parsed.target = endpoint.substr(targetBegin);
    }
    if (parsed.target.empty() || parsed.target.front() != '/' ||
        parsed.target.find('\\') != std::string::npos || hasControlOrWhitespace(parsed.target)) {
        return Result<Endpoint>::failure("LLM endpoint 请求路径非法");
    }

    bool bracketedIpv6 = false;
    bool explicitPort = false;
    std::string portText{"443"};
    if (authority.front() == '[') {
        const auto bracket = authority.find(']');
        if (bracket == std::string::npos || bracket == 1U) {
            return Result<Endpoint>::failure("LLM endpoint IPv6 主机格式非法");
        }
        parsed.host = authority.substr(1U, bracket - 1U);
        bracketedIpv6 = true;
        in6_addr ipv6{};
        if (inet_pton(AF_INET6, parsed.host.c_str(), &ipv6) != 1) {
            return Result<Endpoint>::failure("LLM endpoint IPv6 主机格式非法");
        }
        if (bracket + 1U < authority.size()) {
            if (authority[bracket + 1U] != ':') {
                return Result<Endpoint>::failure("LLM endpoint IPv6 主机后只能跟端口");
            }
            portText = authority.substr(bracket + 2U);
            explicitPort = true;
        }
        if (authority.find('[', 1U) != std::string::npos ||
            authority.find(']', bracket + 1U) != std::string::npos) {
            return Result<Endpoint>::failure("LLM endpoint IPv6 主机格式非法");
        }
    } else {
        const auto colon = authority.find(':');
        if (colon == std::string::npos) {
            parsed.host = authority;
        } else {
            if (authority.find(':', colon + 1U) != std::string::npos) {
                return Result<Endpoint>::failure("LLM endpoint IPv6 地址必须使用方括号");
            }
            parsed.host = authority.substr(0U, colon);
            portText = authority.substr(colon + 1U);
            explicitPort = true;
        }
        if (!validDnsHost(parsed.host)) {
            return Result<Endpoint>::failure("LLM endpoint 主机名非法");
        }
    }

    if (parsed.host.empty() || hasControlOrWhitespace(parsed.host) ||
        parsed.host.find('%') != std::string::npos) {
        return Result<Endpoint>::failure("LLM endpoint 主机名非法");
    }
    auto port = parsePort(portText);
    if (!port.ok()) {
        return Result<Endpoint>::failure(port.error());
    }
    parsed.port = std::move(port.value());
    const auto authorityHost = bracketedIpv6 ? "[" + parsed.host + "]" : parsed.host;
    parsed.hostHeader = authorityHost;
    if (explicitPort && parsed.port != "443") {
        parsed.hostHeader += ":" + parsed.port;
    }

    return Result<Endpoint>::success(parsed);
}

} // namespace cc
