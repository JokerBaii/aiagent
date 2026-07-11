#include "cc/inventory/SecretScanner.hpp"

#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <regex>
#include <string>

namespace cc {
namespace {

[[nodiscard]] bool tokenInName(std::string_view name, std::string_view token) {
    auto position = name.find(token);
    while (position != std::string_view::npos) {
        const auto before = position == 0U ? '\0' : name[position - 1U];
        const auto afterPosition = position + token.size();
        const auto after = afterPosition == name.size() ? '\0' : name[afterPosition];
        const auto delimiter = [](char value) {
            return value == '\0' || std::isalnum(static_cast<unsigned char>(value)) == 0;
        };
        if (delimiter(before) && delimiter(after)) {
            return true;
        }
        position = name.find(token, position + 1U);
    }
    return false;
}

[[nodiscard]] bool sensitiveComponent(std::string name) {
    name = util::lowerAscii(std::move(name));
    const std::filesystem::path component{name};
    const auto extension = component.extension().generic_string();
    if (name == ".env" || name.starts_with(".env.") || name.starts_with(".env-") ||
        name == ".envrc" || name == ".npmrc" || name == ".pypirc" || name == ".netrc" ||
        name == "credentials" || name == "credentials.json" || name == "secrets.json" ||
        name == "service-account.json" || name == "application_default_credentials.json" ||
        name == "id_rsa" || name == "id_ed25519" || name == "id_ecdsa") {
        return true;
    }
    if (extension == ".pem" || extension == ".key" || extension == ".p12" ||
        extension == ".pfx" || extension == ".jks" || extension == ".keystore") {
        return true;
    }
    constexpr std::array<std::string_view, 8> tokens{
        "credential", "credentials", "secret",      "secrets",
        "private-key", "private_key", "access-token", "access_token",
    };
    return std::any_of(tokens.begin(), tokens.end(), [&](std::string_view token) {
        return tokenInName(name, token);
    });
}

[[nodiscard]] std::string unquote(std::string value) {
    value = util::trim(std::move(value));
    while (!value.empty() && (value.back() == ',' || value.back() == ';')) {
        value = util::trim(value.substr(0U, value.size() - 1U));
    }
    if (value.size() >= 2U &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return util::trim(value.substr(1U, value.size() - 2U));
    }
    return value;
}

[[nodiscard]] bool maskedValue(std::string_view value) {
    if (value.size() < 3U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return character == '*' || character == 'x' || character == 'X' || character == '.' ||
               character == '-';
    });
}

[[nodiscard]] bool placeholderValue(std::string value) {
    value = util::lowerAscii(unquote(std::move(value)));
    if (value.empty() || value == "null" || value == "none" || value == "false" ||
        value == "true" || value == "changeme" || value == "change_me" ||
        value == "change-me" || value == "example" || value == "placeholder" ||
        value == "redacted" || value == "masked" || value == "dummy" || value == "fake" ||
        value == "sample" || value == "test" || value == "todo" || value == "tbd" ||
        value == "string" || value == "value" || value == "your-value-here" ||
        maskedValue(value)) {
        return true;
    }
    constexpr std::array<std::string_view, 16> prefixes{
        "${",          "{{",          "<",          "your_",
        "your-",       "example_",    "example-",   "dummy_",
        "dummy-",      "fake_",       "fake-",      "sample_",
        "sample-",     "process.env", "os.getenv",  "getenv(",
    };
    if (std::any_of(prefixes.begin(), prefixes.end(), [&](std::string_view prefix) {
            return value.starts_with(prefix);
        })) {
        return true;
    }
    if (value.starts_with("sk-test-") || value.starts_with("sk_test_") ||
        value.ends_with("_here") || value.ends_with("-here") ||
        value.find("example.com") != std::string::npos) {
        return true;
    }
    return value.size() >= 6U &&
           std::all_of(value.begin(), value.end(), [](char character) {
               return character == 'x' || character == 'X';
           });
}

[[nodiscard]] std::string compactKey(std::string key) {
    key = util::lowerAscii(util::trim(std::move(key)));
    std::string compact;
    compact.reserve(key.size());
    for (const auto character : key) {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
            compact.push_back(character);
        }
    }
    return compact;
}

[[nodiscard]] bool credentialKey(std::string_view key) {
    constexpr std::array<std::string_view, 15> exactKeys{
        "password",       "passwd",          "apikey",       "accesstoken",
        "refreshtoken",   "clientsecret",    "privatekey",    "secretkey",
        "authtoken",      "bearertoken",     "awssecretaccesskey",
        "authorization",  "databaseurl",     "connectionstring", "token",
    };
    if (std::find(exactKeys.begin(), exactKeys.end(), key) != exactKeys.end()) {
        return true;
    }
    constexpr std::array<std::string_view, 7> suffixes{
        "password", "apikey", "accesstoken", "refreshtoken",
        "clientsecret", "authtoken", "authorization",
    };
    return std::any_of(suffixes.begin(), suffixes.end(), [&](std::string_view suffix) {
        return key.size() > suffix.size() && key.ends_with(suffix);
    });
}

[[nodiscard]] bool databaseValueContainsCredential(std::string_view value) {
    const auto scheme = value.find("://");
    const auto at = value.find('@', scheme == std::string_view::npos ? 0U : scheme + 3U);
    if (scheme != std::string_view::npos && at != std::string_view::npos) {
        return value.substr(scheme + 3U, at - scheme - 3U).find(':') != std::string_view::npos;
    }
    const auto lower = util::lowerAscii(std::string{value});
    return lower.find("password=") != std::string::npos ||
           lower.find("pwd=") != std::string::npos;
}

[[nodiscard]] bool credentialAssignment(const std::string& sample) {
    for (const auto& rawLine : util::splitLines(sample)) {
        const auto line = util::trim(rawLine);
        if (line.empty() || line.starts_with('#') || line.starts_with("//")) {
            continue;
        }
        const auto delimiter = line.find_first_of("=:");
        if (delimiter == std::string::npos) {
            continue;
        }
        const auto key = compactKey(line.substr(0U, delimiter));
        if (!credentialKey(key)) {
            continue;
        }
        auto value = unquote(line.substr(delimiter + 1U));
        if (placeholderValue(value)) {
            continue;
        }
        if (key.ends_with("databaseurl") || key.ends_with("connectionstring")) {
            if (databaseValueContainsCredential(value)) {
                return true;
            }
            continue;
        }
        const auto lowerValue = util::lowerAscii(value);
        if (key.ends_with("authorization") && lowerValue.starts_with("bearer ")) {
            value = util::trim(value.substr(7U));
            if (placeholderValue(value)) {
                continue;
            }
        }
        if (value.size() >= 4U) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool tokenPattern(const std::string& sample) {
    static const std::regex pattern{
        R"((A(?:KI|SI)A[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9_-]{20,}|AIza[0-9A-Za-z_-]{30,}|xox[baprs]-[0-9A-Za-z-]{10,}|eyJ[A-Za-z0-9_-]{12,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}))"};
    for (std::sregex_iterator iter(sample.begin(), sample.end(), pattern), end; iter != end;
         ++iter) {
        if (!placeholderValue(iter->str())) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool bearerToken(const std::string& sample) {
    const auto lower = util::lowerAscii(sample);
    std::size_t position = 0U;
    while ((position = lower.find("bearer ", position)) != std::string::npos) {
        const auto start = position + 7U;
        const auto end = sample.find_first_of(" \t\r\n\"'", start);
        const auto value = sample.substr(start, end == std::string::npos ? end : end - start);
        if (value.size() >= 8U && !placeholderValue(value)) {
            return true;
        }
        position = start;
    }
    return false;
}

[[nodiscard]] bool sensitiveText(const std::string& sample) {
    const auto lower = util::lowerAscii(sample);
    if (lower.find("-----begin private key-----") != std::string::npos ||
        lower.find("-----begin rsa private key-----") != std::string::npos ||
        lower.find("-----begin ec private key-----") != std::string::npos ||
        lower.find("-----begin openssh private key-----") != std::string::npos) {
        return true;
    }
    return tokenPattern(sample) || bearerToken(sample) || credentialAssignment(sample);
}

[[nodiscard]] std::string boundedSample(std::string_view text, std::size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return std::string{text};
    }
    const auto headBytes = (maxBytes + 1U) / 2U;
    const auto tailBytes = maxBytes - headBytes;
    std::string sample{text.substr(0U, headBytes)};
    if (tailBytes > 0U) {
        sample.push_back('\n');
        sample.append(text.substr(text.size() - tailBytes));
    }
    return sample;
}

} // namespace

bool SecretScanner::hasSensitivePath(const std::filesystem::path& path) const {
    for (const auto& component : path) {
        if (sensitiveComponent(component.generic_string())) {
            return true;
        }
    }
    const auto normalized = util::lowerAscii(path.lexically_normal().generic_string());
    return normalized.find(".aws/credentials") != std::string::npos ||
           normalized.find(".docker/config.json") != std::string::npos ||
           normalized.find(".kube/config") != std::string::npos ||
           normalized.find(".config/gcloud/application_default_credentials.json") !=
               std::string::npos;
}

SecretScanResult SecretScanner::scanText(std::string_view text, std::size_t maxBytes) const {
    const auto truncated = text.size() > maxBytes;
    const auto sample = boundedSample(text, maxBytes);
    return {.sensitive = sensitiveText(sample), .truncated = truncated};
}

SecretScanResult SecretScanner::scanFileContent(const std::filesystem::path& path,
                                                std::size_t maxBytes) const {
    std::ifstream input(path, std::ios::binary);
    if (!input || maxBytes == 0U) {
        return {};
    }
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0) {
        return {};
    }
    const auto size = static_cast<std::uintmax_t>(end);
    const auto bounded = static_cast<std::uintmax_t>(maxBytes);
    std::string sample;
    if (size <= bounded) {
        sample.resize(static_cast<std::size_t>(size));
        input.seekg(0, std::ios::beg);
        input.read(sample.data(), static_cast<std::streamsize>(sample.size()));
        sample.resize(static_cast<std::size_t>(input.gcount()));
    } else {
        const auto headBytes = (maxBytes + 1U) / 2U;
        const auto tailBytes = maxBytes - headBytes;
        sample.resize(headBytes);
        input.seekg(0, std::ios::beg);
        input.read(sample.data(), static_cast<std::streamsize>(headBytes));
        sample.resize(static_cast<std::size_t>(input.gcount()));
        if (tailBytes > 0U) {
            std::string tail(tailBytes, '\0');
            input.clear();
            input.seekg(-static_cast<std::streamoff>(tailBytes), std::ios::end);
            input.read(tail.data(), static_cast<std::streamsize>(tailBytes));
            tail.resize(static_cast<std::size_t>(input.gcount()));
            sample.push_back('\n');
            sample += tail;
        }
    }
    return {.sensitive = sensitiveText(sample), .truncated = size > bounded};
}

bool SecretScanner::isSensitiveFile(const std::filesystem::path& path) const {
    return hasSensitivePath(path) || scanFileContent(path).sensitive;
}

} // namespace cc
