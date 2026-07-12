/**
 * @file AgentFilePolicy.cpp
 * @brief Agent 文件读取安全策略实现。
 */

#include "cc/agent/AgentFilePolicy.hpp"

#include "cc/inventory/FormatDetector.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>

namespace cc::agent_file_policy {
namespace {

constexpr std::size_t kSensitiveSampleLimit = 32U * 1024U;

[[nodiscard]] std::size_t utf8PrefixLength(std::string_view value, std::size_t limit) {
    auto length = std::min(value.size(), limit);
    while (length > 0U && length < value.size() &&
           (static_cast<unsigned char>(value[length]) & 0xC0U) == 0x80U) {
        --length;
    }
    return length;
}

[[nodiscard]] bool hasTextLikeExtension(const std::filesystem::path& path) {
    const auto ext = util::lowerAscii(path.extension().generic_string());
    return ext.empty() || isLikelyTextExtension(ext) || isCodeExtension(ext);
}

[[nodiscard]] bool sampleLooksBinary(const std::filesystem::path& path) {
    const auto sample = util::readFileLimited(path, 8192U);
    if (sample.empty()) {
        return false;
    }
    std::size_t suspicious = 0U;
    for (const auto byte : sample) {
        const auto value = static_cast<unsigned char>(byte);
        if (value == 0U) {
            return true;
        }
        if (value < 0x09U || (value > 0x0DU && value < 0x20U)) {
            ++suspicious;
        }
    }
    return suspicious * 20U > sample.size();
}

} // namespace

bool hasSensitivePathComponent(const std::filesystem::path& path) {
    for (const auto& component : path) {
        const auto name = util::lowerAscii(component.generic_string());
        const std::filesystem::path componentPath{name};
        const auto extension = componentPath.extension().generic_string();
        if (name.rfind(".env", 0U) == 0U || name == ".npmrc" || name == ".pypirc" ||
            name == ".netrc" || name == "id_rsa" || name == "id_ed25519" || name == "credentials" ||
            name == "credentials.json" || name == "service-account.json" || extension == ".pem" ||
            extension == ".key" || extension == ".p12" || extension == ".pfx" ||
            util::contains(name, "credential") || util::contains(name, "private-key") ||
            util::contains(name, "private_key") || util::contains(name, "secret") ||
            util::contains(name, "access-token") || util::contains(name, "access_token")) {
            return true;
        }
    }
    return false;
}

bool textContainsSecretMarker(std::string sample) {
    sample = util::lowerAscii(std::move(sample));
    static constexpr std::string_view markers[] = {"-----begin private key",
                                                   "-----begin rsa private key",
                                                   "-----begin openssh private key",
                                                   "client_secret",
                                                   "api_key=",
                                                   "api-key:",
                                                   "apikey=",
                                                   "access_token",
                                                   "refresh_token",
                                                   "authorization: bearer",
                                                   "aws_secret_access_key",
                                                   "password=",
                                                   "\"password\":",
                                                   "\"secret\":"};
    return std::any_of(std::begin(markers), std::end(markers), [&](std::string_view marker) {
        return sample.find(marker) != std::string::npos;
    });
}

std::string sanitizeUtf8(std::string_view value) {
    std::string clean;
    clean.reserve(value.size());
    for (std::size_t index = 0U; index < value.size();) {
        const auto lead = static_cast<unsigned char>(value[index]);
        std::size_t width = 0U;
        if (lead <= 0x7FU) {
            width = 1U;
        } else if (lead >= 0xC2U && lead <= 0xDFU) {
            width = 2U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            width = 3U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            width = 4U;
        }
        bool valid = width != 0U && index + width <= value.size();
        for (std::size_t offset = 1U; valid && offset < width; ++offset) {
            valid = (static_cast<unsigned char>(value[index + offset]) & 0xC0U) == 0x80U;
        }
        if (valid && width == 3U) {
            const auto second = static_cast<unsigned char>(value[index + 1U]);
            valid = !((lead == 0xE0U && second < 0xA0U) || (lead == 0xEDU && second >= 0xA0U));
        }
        if (valid && width == 4U) {
            const auto second = static_cast<unsigned char>(value[index + 1U]);
            valid = !((lead == 0xF0U && second < 0x90U) || (lead == 0xF4U && second >= 0x90U));
        }
        if (!valid) {
            clean.push_back('?');
            ++index;
            continue;
        }
        clean.append(value.substr(index, width));
        index += width;
    }
    return clean;
}

std::string truncateText(const std::string& value, std::size_t limit) {
    if (value.size() <= limit) {
        return sanitizeUtf8(value);
    }
    return sanitizeUtf8(value.substr(0U, utf8PrefixLength(value, limit))) + "\n...[已截断]";
}

bool isReadableTextLike(const std::filesystem::path& path) {
    std::error_code ec;
    return hasTextLikeExtension(path) && std::filesystem::is_regular_file(path, ec) &&
           !sampleLooksBinary(path);
}

bool isSensitiveFile(const std::filesystem::path& path) {
    std::error_code ec;
    if (hasSensitivePathComponent(path)) {
        return true;
    }
    return std::filesystem::is_regular_file(path, ec) &&
           textContainsSecretMarker(util::readFileLimited(path, kSensitiveSampleLimit));
}

} // namespace cc::agent_file_policy
