/**
 * @file LlmPromptGuard.cpp
 * @brief LLM 上下文约束实现。
 */

#include "cc/llm/LlmPromptGuard.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <utility>

namespace cc {
namespace {

constexpr std::string_view kRedacted{"[REDACTED]"};
constexpr std::string_view kTruncated{"\n...[context truncated]"};

[[nodiscard]] std::string normalizedRole(const std::string& role) {
    const auto lowered = util::lowerAscii(role);
    if (lowered == "system" || lowered == "assistant" || lowered == "user") {
        return lowered;
    }
    return "user";
}

[[nodiscard]] std::size_t utf8PrefixLength(std::string_view text, std::size_t limit) {
    if (text.size() <= limit) {
        return text.size();
    }
    auto end = limit;
    while (end > 0U && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
        --end;
    }
    if (end == 0U) {
        return 0U;
    }
    const auto lead = static_cast<unsigned char>(text[end]);
    std::size_t expected = 1U;
    if ((lead & 0xE0U) == 0xC0U) {
        expected = 2U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        expected = 3U;
    } else if ((lead & 0xF8U) == 0xF0U) {
        expected = 4U;
    }
    return end + expected <= limit ? limit : end;
}

[[nodiscard]] std::string boundedText(const std::string& text, std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    if (limit <= kTruncated.size()) {
        return std::string{kTruncated.substr(0U, limit)};
    }
    const auto contentLimit = limit - kTruncated.size();
    const auto prefix = utf8PrefixLength(text, contentLimit);
    return text.substr(0U, prefix) + std::string{kTruncated};
}

[[nodiscard]] bool tokenCharacter(char character) {
    const auto byte = static_cast<unsigned char>(character);
    return std::isalnum(byte) != 0 || character == '_' || character == '-' || character == '.' ||
           character == '/' || character == '+' || character == '=';
}

void redactPrefixedTokens(std::string& text) {
    static constexpr std::array<std::string_view, 13> prefixes{
        "github_pat_", "sk-ant-", "glpat-", "xoxb-", "xoxp-", "xoxa-",   "AIza",
        "ya29.",       "ghp_",    "gho_",   "hf_",   "sk-",   "sk_live_"};
    for (const auto prefix : prefixes) {
        std::size_t offset = 0U;
        while ((offset = text.find(prefix, offset)) != std::string::npos) {
            auto end = offset + prefix.size();
            while (end < text.size() && tokenCharacter(text[end])) {
                ++end;
            }
            if (end - offset < prefix.size() + 6U) {
                offset = end;
                continue;
            }
            text.replace(offset, end - offset, kRedacted);
            offset += kRedacted.size();
        }
    }
}

void redactBearerTokens(std::string& text) {
    auto lowered = util::lowerAscii(text);
    std::size_t offset = 0U;
    while ((offset = lowered.find("bearer ", offset)) != std::string::npos) {
        const auto valueBegin = offset + 7U;
        auto valueEnd = valueBegin;
        while (valueEnd < text.size() && tokenCharacter(text[valueEnd])) {
            ++valueEnd;
        }
        if (valueEnd - valueBegin >= 6U) {
            text.replace(valueBegin, valueEnd - valueBegin, kRedacted);
            lowered.replace(valueBegin, valueEnd - valueBegin, kRedacted);
            offset = valueBegin + kRedacted.size();
        } else {
            offset = valueEnd;
        }
    }
}

void redactJwtTokens(std::string& text) {
    std::size_t offset = 0U;
    while (offset < text.size()) {
        while (offset < text.size() && !tokenCharacter(text[offset])) {
            ++offset;
        }
        auto end = offset;
        while (end < text.size() && tokenCharacter(text[end])) {
            ++end;
        }
        const auto candidate = std::string_view{text}.substr(offset, end - offset);
        const auto firstDot = candidate.find('.');
        const auto secondDot = firstDot == std::string_view::npos
                                   ? std::string_view::npos
                                   : candidate.find('.', firstDot + 1U);
        if (candidate.size() >= 24U && firstDot >= 6U && secondDot != std::string_view::npos &&
            secondDot - firstDot >= 7U && candidate.size() - secondDot >= 7U &&
            candidate.find('.', secondDot + 1U) == std::string_view::npos) {
            text.replace(offset, end - offset, kRedacted);
            offset += kRedacted.size();
        } else {
            offset = end;
        }
    }
}

void redactAwsAccessKeys(std::string& text) {
    std::size_t offset = 0U;
    while ((offset = text.find("AKIA", offset)) != std::string::npos) {
        if (text.size() - offset >= 20U &&
            std::all_of(text.begin() + static_cast<std::ptrdiff_t>(offset + 4U),
                        text.begin() + static_cast<std::ptrdiff_t>(offset + 20U),
                        [](char character) {
                            return (character >= 'A' && character <= 'Z') ||
                                   (character >= '0' && character <= '9');
                        })) {
            text.replace(offset, 20U, kRedacted);
            offset += kRedacted.size();
        } else {
            offset += 4U;
        }
    }
}

void redactPrivateKeys(std::string& text) {
    std::size_t begin = 0U;
    while ((begin = text.find("-----BEGIN ", begin)) != std::string::npos) {
        const auto labelEnd = text.find("PRIVATE KEY-----", begin);
        if (labelEnd == std::string::npos || labelEnd - begin > 64U) {
            begin += 11U;
            continue;
        }
        const auto end = text.find("-----END ", labelEnd);
        if (end == std::string::npos) {
            text.replace(begin, text.size() - begin, kRedacted);
            return;
        }
        const auto endMarker = text.find("-----", end + 9U);
        const auto replaceEnd = endMarker == std::string::npos ? text.size() : endMarker + 5U;
        text.replace(begin, replaceEnd - begin, kRedacted);
        begin += kRedacted.size();
    }
}

[[nodiscard]] bool credentialLabel(std::string_view prefix) {
    const auto lowered = util::lowerAscii(std::string{prefix});
    static constexpr std::array<std::string_view, 16> markers{
        "api_key",       "apikey",        "api-key",     "access_token", "auth_token", "authtoken",
        "authorization", "client_secret", "private_key", "password",     "passwd",     "secret_key",
        "accesskey",     "_token",        "token",       "secret"};
    return std::any_of(markers.begin(), markers.end(), [&](std::string_view marker) {
        return lowered.find(marker) != std::string::npos;
    });
}

void redactCredentialLines(std::string& text) {
    std::size_t lineStart = 0U;
    while (lineStart < text.size()) {
        auto logicalEnd = text.find('\n', lineStart);
        const auto hasNewline = logicalEnd != std::string::npos;
        if (!hasNewline) {
            logicalEnd = text.size();
        }
        auto fieldStart = lineStart;
        while (fieldStart < logicalEnd) {
            const auto separator = text.find_first_of(":=", fieldStart);
            if (separator == std::string::npos || separator >= logicalEnd) {
                break;
            }
            auto labelStart = text.find_last_of(",{; \t", separator);
            labelStart = labelStart == std::string::npos || labelStart < lineStart
                             ? lineStart
                             : labelStart + 1U;
            if (separator - labelStart > 160U || !credentialLabel(std::string_view{text}.substr(
                                                     labelStart, separator - labelStart))) {
                fieldStart = separator + 1U;
                continue;
            }

            auto valueBegin = separator + 1U;
            while (valueBegin < logicalEnd &&
                   (text[valueBegin] == ' ' || text[valueBegin] == '\t')) {
                ++valueBegin;
            }
            auto valueEnd = logicalEnd;
            if (valueBegin < valueEnd && (text[valueBegin] == '"' || text[valueBegin] == '\'')) {
                const auto quote = text[valueBegin];
                ++valueBegin;
                const auto closing = text.find(quote, valueBegin);
                if (closing != std::string::npos && closing <= logicalEnd) {
                    valueEnd = closing;
                }
            } else {
                const auto comma = text.find_first_of(",;", valueBegin);
                if (comma != std::string::npos && comma < logicalEnd) {
                    valueEnd = comma;
                }
            }
            const auto oldLength = valueEnd - valueBegin;
            text.replace(valueBegin, oldLength, kRedacted);
            const auto delta = static_cast<std::ptrdiff_t>(kRedacted.size()) -
                               static_cast<std::ptrdiff_t>(oldLength);
            logicalEnd = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(logicalEnd) + delta);
            fieldStart = valueBegin + kRedacted.size();
        }
        if (!hasNewline) {
            return;
        }
        lineStart = logicalEnd + 1U;
    }
}

void redactUriUserInfo(std::string& text) {
    std::size_t scheme = 0U;
    while ((scheme = text.find("://", scheme)) != std::string::npos) {
        const auto credentials = scheme + 3U;
        const auto authorityEnd = text.find_first_of("/ \t\r\n", credentials);
        const auto at = text.find('@', credentials);
        if (at != std::string::npos && (authorityEnd == std::string::npos || at < authorityEnd)) {
            text.replace(credentials, at - credentials, kRedacted);
            scheme = credentials + kRedacted.size() + 1U;
        } else {
            scheme = credentials;
        }
    }
}

} // namespace

std::string LlmPromptGuard::redactSecrets(std::string text) const {
    redactPrivateKeys(text);
    redactCredentialLines(text);
    redactUriUserInfo(text);
    redactBearerTokens(text);
    redactPrefixedTokens(text);
    redactAwsAccessKeys(text);
    redactJwtTokens(text);
    return text;
}

std::vector<LlmMessage> LlmPromptGuard::sanitize(const std::vector<LlmMessage>& messages,
                                                 const LlmPromptBudget& budget) const {
    if (messages.empty() || budget.maxMessages == 0U || budget.maxMessageBytes == 0U ||
        budget.maxTotalBytes == 0U) {
        return {};
    }

    const auto first =
        messages.size() > budget.maxMessages ? messages.size() - budget.maxMessages : 0U;
    std::vector<std::size_t> indices;
    indices.reserve(std::min(messages.size(), budget.maxMessages));
    for (std::size_t index = first; index < messages.size(); ++index) {
        indices.push_back(index);
    }
    const auto firstSystem =
        std::find_if(messages.begin(), messages.end(), [](const auto& message) {
            return util::lowerAscii(message.role) == "system";
        });
    if (firstSystem != messages.end()) {
        const auto systemIndex =
            static_cast<std::size_t>(std::distance(messages.begin(), firstSystem));
        if (std::find(indices.begin(), indices.end(), systemIndex) == indices.end()) {
            if (!indices.empty()) {
                indices.front() = systemIndex;
            } else {
                indices.push_back(systemIndex);
            }
            std::sort(indices.begin(), indices.end());
        }
    }

    std::vector<std::pair<std::size_t, LlmMessage>> selected;
    selected.reserve(indices.size());
    std::size_t remaining = budget.maxTotalBytes;
    const auto keepMessage = [&](std::size_t index, std::size_t& available, auto& destination) {
        if (available == 0U) {
            return;
        }
        auto content = redactSecrets(messages[index].content);
        content = boundedText(content, std::min(budget.maxMessageBytes, available));
        available -= content.size();
        destination.emplace_back(index, LlmMessage{.role = normalizedRole(messages[index].role),
                                                   .content = std::move(content)});
    };

    std::size_t systemIndex = messages.size();
    if (firstSystem != messages.end()) {
        systemIndex = static_cast<std::size_t>(std::distance(messages.begin(), firstSystem));
        keepMessage(systemIndex, remaining, selected);
    }
    for (auto iter = indices.rbegin(); iter != indices.rend() && remaining > 0U; ++iter) {
        if (*iter != systemIndex) {
            keepMessage(*iter, remaining, selected);
        }
    }
    std::sort(selected.begin(), selected.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    std::vector<LlmMessage> sanitized;
    sanitized.reserve(selected.size());
    for (auto& [index, message] : selected) {
        static_cast<void>(index);
        sanitized.push_back(std::move(message));
    }
    return sanitized;
}

} // namespace cc
