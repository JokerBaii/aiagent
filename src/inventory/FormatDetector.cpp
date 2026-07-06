/**
 * @file FormatDetector.cpp
 * @brief 文件格式和基础元数据检测实现。
 */

#include "cc/inventory/FormatDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>

namespace cc {
namespace {

[[nodiscard]] bool extensionIn(const std::string& extension,
                               const std::vector<std::string>& values) {
    return std::find(values.begin(), values.end(), util::lowerAscii(extension)) != values.end();
}

} // namespace

bool isLikelyTextExtension(const std::string& extension) {
    return extensionIn(extension, {".md",   ".txt", ".json", ".yaml", ".yml",   ".csv",  ".xml",
                                   ".html", ".cpp", ".hpp",  ".h",    ".c",     ".py",   ".js",
                                   ".ts",   ".tsx", ".jsx",  ".qml",  ".cmake", ".toml", ".ini",
                                   ".java", ".go",  ".rs",   ".sql",  ".sh",    ".log"});
}

bool isOfficeExtension(const std::string& extension) {
    return extensionIn(extension, {".docx", ".pptx", ".xlsx"});
}

ProjectAsset FormatDetector::detect(const std::filesystem::path& root,
                                    const std::filesystem::path& file) const {
    ProjectAsset asset;
    asset.absolutePath = file;
    std::error_code ec;
    asset.relativePath = std::filesystem::relative(file, root, ec);
    if (ec) {
        asset.relativePath = file.filename();
    }
    asset.fileName = file.filename().string();
    asset.extension = util::lowerAscii(file.extension().string());
    asset.sizeBytes = std::filesystem::file_size(file, ec);
    if (ec) {
        asset.sizeBytes = 0;
    }

    if (isLikelyTextExtension(asset.extension)) {
        asset.format = asset.extension.empty() ? "text" : asset.extension.substr(1);
        asset.mime = "text/plain";
        asset.auditable = true;
    } else if (isOfficeExtension(asset.extension)) {
        asset.format = asset.extension.substr(1);
        asset.mime = "application/vnd.openxmlformats-officedocument";
        asset.auditable = true;
    } else if (asset.extension == ".pdf") {
        asset.format = "pdf";
        asset.mime = "application/pdf";
        asset.auditable = true;
    } else if (ArchiveExtractor::isArchivePath(file)) {
        asset.format = "archive";
        asset.mime = "application/archive";
    } else {
        asset.format = asset.extension.empty() ? "unknown" : asset.extension.substr(1);
        asset.mime = "application/octet-stream";
    }

    if (extensionIn(asset.extension, {".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts", ".tsx",
                                      ".jsx", ".java", ".go", ".rs", ".qml"})) {
        asset.language = asset.extension.substr(1);
    }
    return asset;
}

} // namespace cc
