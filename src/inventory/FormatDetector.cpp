/**
 * @file FormatDetector.cpp
 * @brief 文件格式和基础元数据检测实现。
 */

#include "cc/inventory/FormatDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <vector>

namespace cc {
namespace {

[[nodiscard]] bool extensionIn(const std::string& extension,
                               const std::vector<std::string>& values) {
    return std::find(values.begin(), values.end(), util::lowerAscii(extension)) != values.end();
}

} // namespace

bool isLikelyTextExtension(const std::string& extension) {
    return extensionIn(extension,
                       {".md",    ".markdown", ".txt",        ".rst",          ".adoc",     ".json",
                        ".jsonl", ".yaml",     ".yml",        ".csv",          ".tsv",      ".xml",
                        ".html",  ".htm",      ".css",        ".scss",         ".less",     ".toml",
                        ".ini",   ".cfg",      ".conf",       ".env",          ".sql",      ".log",
                        ".diff",  ".patch",    ".dockerfile", ".editorconfig", ".gitignore"});
}

bool isCodeExtension(const std::string& extension) {
    return extensionIn(extension,
                       {".c",    ".cc",  ".cpp",   ".cxx",    ".h",    ".hh",    ".hpp",   ".hxx",
                        ".m",    ".mm",  ".py",    ".pyw",    ".js",   ".mjs",   ".cjs",   ".ts",
                        ".tsx",  ".jsx", ".vue",   ".svelte", ".qml",  ".java",  ".kt",    ".kts",
                        ".go",   ".rs",  ".swift", ".cs",     ".fs",   ".fsx",   ".php",   ".rb",
                        ".pl",   ".pm",  ".lua",   ".r",      ".jl",   ".scala", ".dart",  ".ex",
                        ".exs",  ".erl", ".hrl",   ".clj",    ".cljs", ".sh",    ".bash",  ".zsh",
                        ".fish", ".ps1", ".bat",   ".cmd",    ".sql",  ".cmake", ".gradle"});
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

    if (isCodeExtension(asset.extension)) {
        asset.format = asset.extension.substr(1);
        asset.mime = "text/x-source-code";
        asset.language = asset.extension.substr(1);
        asset.auditable = true;
    } else if (isLikelyTextExtension(asset.extension) || asset.extension.empty()) {
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
    } else if (extensionIn(asset.extension, {".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg",
                                             ".bmp", ".tiff", ".ico"})) {
        asset.format = asset.extension.substr(1);
        asset.mime = asset.extension == ".svg" ? "image/svg+xml" : "image/" + asset.format;
    } else if (extensionIn(asset.extension, {".mp4", ".mov", ".avi", ".mkv", ".webm"})) {
        asset.format = asset.extension.substr(1);
        asset.mime = "video/" + asset.format;
    } else if (extensionIn(asset.extension, {".mp3", ".wav", ".flac", ".ogg", ".m4a"})) {
        asset.format = asset.extension.substr(1);
        asset.mime = "audio/" + asset.format;
    } else if (extensionIn(asset.extension, {".onnx", ".pt", ".pth", ".pkl", ".joblib",
                                             ".safetensors", ".tflite", ".pb"})) {
        asset.format = asset.extension.substr(1);
        asset.mime = "application/x-model-artifact";
    } else if (extensionIn(asset.extension, {".exe", ".dll", ".so", ".dylib", ".a", ".o", ".class",
                                             ".jar", ".wasm"})) {
        asset.format = asset.extension.substr(1);
        asset.mime = "application/octet-stream";
    } else {
        asset.format = asset.extension.empty() ? "binary" : asset.extension.substr(1);
        asset.mime = "application/octet-stream";
    }
    return asset;
}

} // namespace cc
