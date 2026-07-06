/**
 * @file StructuredTextExtractor.cpp
 * @brief JSON/YAML 结构化文本抽取实现。
 */

#include "cc/text/StructuredTextExtractor.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <sstream>

namespace cc {
namespace {

void appendJsonValue(const JsonValue& value, const std::string& path, std::ostringstream& output) {
    if (value.isObject()) {
        for (const auto& [key, child] : value.asObject()) {
            const auto nextPath = path.empty() ? key : path + "." + key;
            output << "key: " << nextPath << "\n";
            appendJsonValue(child, nextPath, output);
        }
        return;
    }
    if (value.isArray()) {
        std::size_t index = 0;
        for (const auto& child : value.asArray()) {
            appendJsonValue(child, path + "[" + std::to_string(index) + "]", output);
            ++index;
        }
        return;
    }
    if (value.isString()) {
        output << path << ": " << value.asString() << "\n";
    } else if (value.isNumber()) {
        output << path << ": " << value.asNumber() << "\n";
    } else if (value.isBool()) {
        output << path << ": " << (value.asBool() ? "true" : "false") << "\n";
    }
}

[[nodiscard]] std::string extractYamlLikeText(const std::string& content) {
    std::ostringstream output;
    for (const auto& rawLine : util::splitLines(content)) {
        const auto line = util::trim(rawLine);
        if (line.empty() || line.rfind("#", 0) == 0) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            output << line << "\n";
            continue;
        }
        const auto key = util::trim(line.substr(0, colon));
        const auto value = util::trim(line.substr(colon + 1U));
        if (!key.empty()) {
            output << "key: " << key << "\n";
        }
        if (!value.empty()) {
            output << key << ": " << value << "\n";
        }
    }
    return output.str();
}

} // namespace

bool isStructuredTextExtension(const std::string& extension) {
    const auto lower = util::lowerAscii(extension);
    return lower == ".json" || lower == ".yaml" || lower == ".yml";
}

Result<TextDocument> StructuredTextExtractor::extract(const ProjectAsset& asset) const {
    constexpr std::size_t kMaxStructuredBytes = 512U * 1024U;
    TextDocument document;
    document.sourceFile = asset.relativePath;
    document.title = asset.fileName;
    const auto content = util::readFileLimited(asset.absolutePath, kMaxStructuredBytes);
    if (content.empty()) {
        document.status = "EMPTY_OR_UNREADABLE";
        return Result<TextDocument>::success(std::move(document));
    }

    if (util::lowerAscii(asset.extension) == ".json") {
        auto parsed = parseJson(content);
        if (parsed.ok()) {
            std::ostringstream output;
            appendJsonValue(parsed.value(), {}, output);
            document.text = output.str();
            document.status = "EXTRACTED_STRUCTURED_JSON";
            return Result<TextDocument>::success(std::move(document));
        }
        document.text = content;
        document.status = "NEED_REVIEW_JSON_PARSE_FAILED";
        return Result<TextDocument>::success(std::move(document));
    }

    document.text = extractYamlLikeText(content);
    document.status =
        document.text.empty() ? "NEED_REVIEW_YAML_EMPTY" : "EXTRACTED_STRUCTURED_YAML";
    return Result<TextDocument>::success(std::move(document));
}

} // namespace cc
