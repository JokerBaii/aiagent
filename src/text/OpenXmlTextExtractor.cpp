/**
 * @file OpenXmlTextExtractor.cpp
 * @brief docx/pptx/xlsx OpenXML 文本抽取实现。
 */

#include "cc/text/OpenXmlTextExtractor.hpp"
#include "cc/loader/ZipArchiveReader.hpp"
#include "cc/util/StringUtil.hpp"

#include <pugixml.hpp>

#include <sstream>

namespace cc {
namespace {

[[nodiscard]] std::vector<std::string> listEntries(const ZipArchiveReader& reader,
                                                   const std::filesystem::path& path) {
    std::vector<std::string> entries;
    const auto listed = reader.list(path);
    if (!listed.ok()) {
        return entries;
    }
    for (const auto& entry : listed.value()) {
        if (!entry.directory && !entry.symlink) {
            entries.push_back(entry.relativePath.generic_string());
        }
    }
    return entries;
}

void collectTextNodes(const pugi::xml_node& node, std::ostringstream& text) {
    for (const auto& child : node.children()) {
        if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
            text << child.value() << ' ';
            continue;
        }
        collectTextNodes(child, text);
    }
}

[[nodiscard]] std::string extractXmlText(const std::string& xml) {
    pugi::xml_document document;
    const auto parsed = document.load_buffer(xml.data(), xml.size(), pugi::parse_default);
    if (!parsed) {
        return {};
    }
    std::ostringstream text;
    collectTextNodes(document, text);
    return text.str();
}

[[nodiscard]] std::vector<std::string> targetEntries(const ProjectAsset& asset,
                                                     const ZipArchiveReader& reader) {
    if (asset.extension == ".docx") {
        return {"word/document.xml"};
    }
    if (asset.extension == ".pptx") {
        std::vector<std::string> entries;
        for (const auto& entry : listEntries(reader, asset.absolutePath)) {
            if (entry.rfind("ppt/slides/slide", 0) == 0 && util::contains(entry, ".xml")) {
                entries.push_back(entry);
            }
        }
        return entries;
    }
    if (asset.extension == ".xlsx") {
        std::vector<std::string> entries{"xl/sharedStrings.xml"};
        for (const auto& entry : listEntries(reader, asset.absolutePath)) {
            if (entry.rfind("xl/worksheets/sheet", 0) == 0 && util::contains(entry, ".xml")) {
                entries.push_back(entry);
            }
        }
        return entries;
    }
    return {};
}

} // namespace

Result<TextDocument> OpenXmlTextExtractor::extract(const ProjectAsset& asset) const {
    constexpr std::size_t kMaxOpenXmlBytes = 1024U * 1024U;
    constexpr std::size_t kMaxDocumentTextBytes = 2U * 1024U * 1024U;
    constexpr std::size_t kMaxEntries = 256U;
    TextDocument document;
    document.sourceFile = asset.relativePath;
    document.title = asset.fileName;

    ZipArchiveReader reader;
    std::ostringstream text;
    bool reviewRequired = false;
    std::size_t accumulatedBytes = 0U;
    const auto entries = targetEntries(asset, reader);
    if (entries.size() > kMaxEntries) {
        reviewRequired = true;
    }
    std::size_t processed = 0U;
    for (const auto& entry : entries) {
        if (processed >= kMaxEntries || accumulatedBytes >= kMaxDocumentTextBytes) {
            reviewRequired = true;
            break;
        }
        const auto xml = reader.readTextEntry(
            {.archivePath = asset.absolutePath, .entryPath = entry, .maxBytes = kMaxOpenXmlBytes});
        if (xml.ok() && !xml.value().empty()) {
            const auto extracted = extractXmlText(xml.value());
            if (!extracted.empty()) {
                const auto remaining = kMaxDocumentTextBytes - accumulatedBytes;
                if (extracted.size() > remaining) {
                    text << extracted.substr(0U, remaining);
                    accumulatedBytes += remaining;
                    reviewRequired = true;
                    break;
                }
                text << extracted << '\n';
                accumulatedBytes += extracted.size() + 1U;
            }
        } else {
            reviewRequired = true;
        }
        ++processed;
    }

    document.text = text.str();
    document.status = document.text.empty() || reviewRequired
                          ? "NEED_REVIEW_OPENXML_TEXT_EXTRACTION_LIMITED"
                          : "EXTRACTED_OPENXML";
    return Result<TextDocument>::success(std::move(document));
}

} // namespace cc
