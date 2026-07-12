#include "cc/inventory/FormatDetector.hpp"

#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

namespace cc {
namespace {

constexpr std::size_t kFormatSampleBytes = 16U * 1024U;

enum class FormatKind {
    Code,
    Text,
    OfficeOpenXml,
    Document,
    Pdf,
    Archive,
    Image,
    Video,
    Audio,
    ModelArtifact,
    ThreeDimensionalModel,
    Binary,
    Unknown,
};

struct FormatMetadata {
    FormatKind kind{FormatKind::Unknown};
    std::string format{"unknown"};
    std::string mime{"application/octet-stream"};
    std::string language;
};

[[nodiscard]] bool extensionIn(const std::string& extension,
                               const std::vector<std::string>& values) {
    return std::find(values.begin(), values.end(), util::lowerAscii(extension)) != values.end();
}

[[nodiscard]] bool isKnownTextFileName(const std::filesystem::path& path) {
    const auto name = util::lowerAscii(path.filename().string());
    return name == "dockerfile" || name == "makefile" || name == "gemfile" || name == "rakefile" ||
           name == "license" || name == "copying" || name == "notice" || name == "authors";
}

[[nodiscard]] std::string formatFromExtension(const std::string& extension, std::string fallback) {
    return extension.empty() ? std::move(fallback) : extension.substr(1U);
}

[[nodiscard]] FormatMetadata metadataForPath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    if (ArchiveExtractor::isArchivePath(path)) {
        return {FormatKind::Archive, "archive", "application/archive", {}};
    }
    if (isCodeExtension(extension)) {
        return {FormatKind::Code, formatFromExtension(extension, "source"), "text/x-source-code",
                formatFromExtension(extension, "source")};
    }
    if (isLikelyTextExtension(extension) || isKnownTextFileName(path)) {
        return {FormatKind::Text, formatFromExtension(extension, "text"), "text/plain", {}};
    }
    if (isOfficeExtension(extension)) {
        return {FormatKind::OfficeOpenXml,
                formatFromExtension(extension, "document"),
                "application/vnd.openxmlformats-officedocument",
                {}};
    }
    if (extension == ".pdf") {
        return {FormatKind::Pdf, "pdf", "application/pdf", {}};
    }
    if (extension == ".doc") {
        return {FormatKind::Document, "doc", "application/msword", {}};
    }
    if (extension == ".xls") {
        return {FormatKind::Document, "xls", "application/vnd.ms-excel", {}};
    }
    if (extension == ".ppt") {
        return {FormatKind::Document, "ppt", "application/vnd.ms-powerpoint", {}};
    }
    if (extension == ".rtf") {
        return {FormatKind::Document, "rtf", "application/rtf", {}};
    }
    if (extensionIn(extension, {".odt", ".ods", ".odp"})) {
        return {
            FormatKind::Document, extension.substr(1U), "application/vnd.oasis.opendocument", {}};
    }
    if (extensionIn(extension, {".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg", ".bmp", ".tiff",
                                ".tif", ".ico", ".avif", ".heic"})) {
        const auto format = extension.substr(1U);
        return {FormatKind::Image,
                format,
                extension == ".svg" ? "image/svg+xml" : "image/" + format,
                {}};
    }
    if (extensionIn(extension, {".mp4", ".mov", ".avi", ".mkv", ".webm", ".mpeg", ".mpg", ".m4v",
                                ".wmv", ".flv"})) {
        const auto format = extension.substr(1U);
        return {FormatKind::Video, format, "video/" + format, {}};
    }
    if (extensionIn(extension,
                    {".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac", ".opus", ".wma"})) {
        const auto format = extension.substr(1U);
        return {FormatKind::Audio, format, "audio/" + format, {}};
    }
    if (extensionIn(extension, {".onnx", ".pt", ".pth", ".pkl", ".joblib", ".safetensors",
                                ".tflite", ".pb"})) {
        return {
            FormatKind::ModelArtifact, extension.substr(1U), "application/x-model-artifact", {}};
    }
    if (extensionIn(extension,
                    {".glb", ".gltf", ".fbx", ".obj", ".stl", ".dae", ".3ds", ".blend"})) {
        const auto format = extension.substr(1U);
        return {FormatKind::ThreeDimensionalModel, format, "model/" + format, {}};
    }
    if (extensionIn(extension, {".exe", ".dll",  ".so",  ".dylib", ".a",    ".o",      ".class",
                                ".jar", ".wasm", ".bin", ".dat",   ".db",   ".sqlite", ".iso",
                                ".dmg", ".apk",  ".ttf", ".otf",   ".woff", ".woff2"})) {
        return {FormatKind::Binary, extension.substr(1U), "application/octet-stream", {}};
    }
    return {FormatKind::Unknown,
            formatFromExtension(extension, "unknown"),
            "application/octet-stream",
            {}};
}

[[nodiscard]] ProjectAsset metadataAsset(const std::filesystem::path& relativePath,
                                         std::uintmax_t sizeBytes) {
    ProjectAsset asset;
    asset.relativePath = relativePath.lexically_normal();
    asset.fileName = relativePath.filename().string();
    asset.extension = util::lowerAscii(relativePath.extension().string());
    asset.sizeBytes = sizeBytes;
    const auto metadata = metadataForPath(relativePath);
    asset.format = metadata.format;
    asset.mime = metadata.mime;
    asset.language = metadata.language;
    return asset;
}

[[nodiscard]] std::optional<std::vector<unsigned char>>
readSample(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::vector<unsigned char> sample(kFormatSampleBytes);
    input.read(reinterpret_cast<char*>(sample.data()), static_cast<std::streamsize>(sample.size()));
    if (input.bad()) {
        return std::nullopt;
    }
    sample.resize(static_cast<std::size_t>(std::max<std::streamsize>(input.gcount(), 0)));
    return sample;
}

[[nodiscard]] bool startsWith(const std::vector<unsigned char>& sample,
                              std::initializer_list<unsigned char> prefix) {
    return sample.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), sample.begin());
}

[[nodiscard]] bool startsWithAt(const std::vector<unsigned char>& sample, std::size_t offset,
                                std::initializer_list<unsigned char> prefix) {
    return offset <= sample.size() && prefix.size() <= sample.size() - offset &&
           std::equal(prefix.begin(), prefix.end(),
                      sample.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] bool isPdf(const std::vector<unsigned char>& sample) {
    return startsWith(sample, {'%', 'P', 'D', 'F', '-'});
}

[[nodiscard]] bool isZip(const std::vector<unsigned char>& sample) {
    return startsWith(sample, {'P', 'K', 0x03U, 0x04U}) ||
           startsWith(sample, {'P', 'K', 0x05U, 0x06U}) ||
           startsWith(sample, {'P', 'K', 0x07U, 0x08U});
}

[[nodiscard]] std::optional<FormatMetadata>
metadataForMagic(const std::vector<unsigned char>& sample) {
    if (isPdf(sample)) {
        return FormatMetadata{FormatKind::Pdf, "pdf", "application/pdf", {}};
    }
    if (isZip(sample)) {
        return FormatMetadata{FormatKind::Archive, "zip", "application/archive", {}};
    }
    if (startsWith(sample, {0x89U, 'P', 'N', 'G', 0x0dU, 0x0aU, 0x1aU, 0x0aU})) {
        return FormatMetadata{FormatKind::Image, "png", "image/png", {}};
    }
    if (startsWith(sample, {0xffU, 0xd8U, 0xffU})) {
        return FormatMetadata{FormatKind::Image, "jpeg", "image/jpeg", {}};
    }
    if (startsWith(sample, {'G', 'I', 'F', '8'})) {
        return FormatMetadata{FormatKind::Image, "gif", "image/gif", {}};
    }
    if (startsWith(sample, {'R', 'I', 'F', 'F'}) &&
        startsWithAt(sample, 8U, {'W', 'E', 'B', 'P'})) {
        return FormatMetadata{FormatKind::Image, "webp", "image/webp", {}};
    }
    if (startsWith(sample, {'R', 'I', 'F', 'F'}) &&
        startsWithAt(sample, 8U, {'W', 'A', 'V', 'E'})) {
        return FormatMetadata{FormatKind::Audio, "wav", "audio/wav", {}};
    }
    if (startsWith(sample, {'O', 'g', 'g', 'S'})) {
        return FormatMetadata{FormatKind::Audio, "ogg", "audio/ogg", {}};
    }
    if (startsWith(sample, {'f', 'L', 'a', 'C'})) {
        return FormatMetadata{FormatKind::Audio, "flac", "audio/flac", {}};
    }
    if (startsWith(sample, {'I', 'D', '3'})) {
        return FormatMetadata{FormatKind::Audio, "mp3", "audio/mpeg", {}};
    }
    if (startsWithAt(sample, 4U, {'f', 't', 'y', 'p'})) {
        return FormatMetadata{FormatKind::Video, "mp4", "video/mp4", {}};
    }
    if (startsWith(sample, {'g', 'l', 'T', 'F'})) {
        return FormatMetadata{FormatKind::ThreeDimensionalModel, "glb", "model/gltf-binary", {}};
    }
    if (startsWith(sample, {0x1fU, 0x8bU})) {
        return FormatMetadata{FormatKind::Archive, "gzip", "application/archive", {}};
    }
    if (startsWith(sample, {'7', 'z', 0xbcU, 0xafU, 0x27U, 0x1cU})) {
        return FormatMetadata{FormatKind::Archive, "7z", "application/archive", {}};
    }
    if (startsWith(sample, {0x28U, 0xb5U, 0x2fU, 0xfdU})) {
        return FormatMetadata{FormatKind::Archive, "zstd", "application/archive", {}};
    }
    if (startsWith(sample, {'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f', 'o', 'r', 'm', 'a', 't', ' ',
                            '3', 0U})) {
        return FormatMetadata{FormatKind::Binary, "sqlite", "application/x-sqlite3", {}};
    }
    if (startsWith(sample, {0x7fU, 'E', 'L', 'F'})) {
        return FormatMetadata{FormatKind::Binary, "elf", "application/x-executable", {}};
    }
    if (startsWith(sample, {'M', 'Z'})) {
        return FormatMetadata{FormatKind::Binary, "pe", "application/x-executable", {}};
    }
    return std::nullopt;
}

[[nodiscard]] bool validUtf8(const std::vector<unsigned char>& sample) {
    std::size_t index = 0U;
    while (index < sample.size()) {
        const auto byte = sample[index];
        if (byte < 0x80U) {
            ++index;
            continue;
        }
        std::size_t length = 0U;
        std::uint32_t codePoint = 0U;
        if ((byte & 0xe0U) == 0xc0U) {
            length = 2U;
            codePoint = static_cast<std::uint32_t>(byte & 0x1fU);
        } else if ((byte & 0xf0U) == 0xe0U) {
            length = 3U;
            codePoint = static_cast<std::uint32_t>(byte & 0x0fU);
        } else if ((byte & 0xf8U) == 0xf0U) {
            length = 4U;
            codePoint = static_cast<std::uint32_t>(byte & 0x07U);
        } else {
            return false;
        }
        if (index + length > sample.size()) {
            return false;
        }
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation = sample[index + offset];
            if ((continuation & 0xc0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | static_cast<std::uint32_t>(continuation & 0x3fU);
        }
        if ((length == 2U && codePoint < 0x80U) || (length == 3U && codePoint < 0x800U) ||
            (length == 4U && codePoint < 0x10000U) || codePoint > 0x10ffffU ||
            (codePoint >= 0xd800U && codePoint <= 0xdfffU)) {
            return false;
        }
        index += length;
    }
    return true;
}

[[nodiscard]] bool looksLikeText(const std::vector<unsigned char>& sample) {
    if (sample.empty()) {
        return true;
    }
    if (metadataForMagic(sample).has_value() || !validUtf8(sample)) {
        return false;
    }
    std::size_t controls = 0U;
    for (const auto byte : sample) {
        if (byte == 0U) {
            return false;
        }
        if (byte < 0x20U && byte != '\n' && byte != '\r' && byte != '\t' && byte != '\f') {
            ++controls;
        }
    }
    return controls * 100U <= sample.size();
}

void markMismatch(ProjectAsset& asset, std::string detectedFormat, std::string detectedMime) {
    asset.format = std::move(detectedFormat);
    asset.mime = std::move(detectedMime);
    asset.auditable = false;
    asset.riskFlags.push_back("EXTENSION_CONTENT_MISMATCH");
}

void applyDetectedMetadata(ProjectAsset& asset, const FormatMetadata& metadata) {
    asset.format = metadata.format;
    asset.mime = metadata.mime;
    asset.language = metadata.language;
}

} // namespace

bool isLikelyTextExtension(const std::string& extension) {
    return extensionIn(extension,
                       {".md",         ".markdown",   ".txt",          ".rst",       ".adoc",
                        ".json",       ".jsonl",      ".yaml",         ".yml",       ".csv",
                        ".tsv",        ".xml",        ".html",         ".htm",       ".css",
                        ".scss",       ".less",       ".toml",         ".ini",       ".cfg",
                        ".conf",       ".env",        ".sql",          ".log",       ".diff",
                        ".patch",      ".dockerfile", ".editorconfig", ".gitignore", ".lock",
                        ".properties", ".graphql",    ".gql",          ".tex",       ".bib"});
}

bool isCodeExtension(const std::string& extension) {
    return extensionIn(
        extension,
        {".c",      ".cc",     ".cpp",  ".cxx",  ".h",   ".hh",  ".hpp", ".hxx", ".m",
         ".mm",     ".py",     ".pyw",  ".js",   ".mjs", ".cjs", ".ts",  ".tsx", ".jsx",
         ".vue",    ".svelte", ".qml",  ".java", ".kt",  ".kts", ".go",  ".rs",  ".swift",
         ".cs",     ".fs",     ".fsx",  ".php",  ".rb",  ".pl",  ".pm",  ".lua", ".r",
         ".jl",     ".scala",  ".dart", ".ex",   ".exs", ".erl", ".hrl", ".clj", ".cljs",
         ".sh",     ".bash",   ".zsh",  ".fish", ".ps1", ".bat", ".cmd", ".sql", ".cmake",
         ".gradle", ".proto",  ".sol",  ".asm",  ".s",   ".v",   ".vhd", ".vhdl"});
}

bool isOfficeExtension(const std::string& extension) {
    return extensionIn(extension, {".docx", ".pptx", ".xlsx"});
}

ProjectAsset FormatDetector::detect(const std::filesystem::path& root,
                                    const std::filesystem::path& file) const {
    std::error_code error;
    auto relativePath = std::filesystem::relative(file, root, error);
    if (error) {
        relativePath = file.filename();
    }
    auto sizeBytes = std::filesystem::file_size(file, error);
    if (error) {
        sizeBytes = 0U;
    }
    auto asset = metadataAsset(relativePath, sizeBytes);
    asset.absolutePath = file;

    const auto sample = readSample(file);
    if (!sample.has_value()) {
        asset.riskFlags.push_back("CONTENT_UNREADABLE");
        return asset;
    }
    const bool textContent = looksLikeText(*sample);
    const bool pdfContent = isPdf(*sample);
    const bool zipContent = isZip(*sample);
    const auto kind = metadataForPath(relativePath).kind;
    const auto magic = metadataForMagic(*sample);

    if (kind == FormatKind::Code || kind == FormatKind::Text) {
        if (!textContent) {
            markMismatch(asset, magic.has_value() ? magic->format : "binary",
                         magic.has_value() ? magic->mime : "application/octet-stream");
            return asset;
        }
        asset.auditable = true;
        return asset;
    }
    if (kind == FormatKind::OfficeOpenXml) {
        if (!zipContent) {
            markMismatch(asset,
                         magic.has_value() ? magic->format
                         : textContent     ? "text"
                                           : "binary",
                         magic.has_value() ? magic->mime
                         : textContent     ? "text/plain"
                                           : "application/octet-stream");
            return asset;
        }
        asset.auditable = true;
        return asset;
    }
    if (kind == FormatKind::Pdf) {
        if (!pdfContent) {
            markMismatch(asset,
                         magic.has_value() ? magic->format
                         : textContent     ? "text"
                                           : "binary",
                         magic.has_value() ? magic->mime
                         : textContent     ? "text/plain"
                                           : "application/octet-stream");
            return asset;
        }
        asset.auditable = true;
        return asset;
    }
    if (kind == FormatKind::Archive) {
        if (textContent && !zipContent) {
            asset.riskFlags.push_back("EXTENSION_CONTENT_MISMATCH");
        }
        return asset;
    }
    if (kind == FormatKind::Image) {
        if (asset.extension != ".svg" && textContent) {
            asset.riskFlags.push_back("EXTENSION_CONTENT_MISMATCH");
        }
        return asset;
    }
    if (kind != FormatKind::Unknown) {
        const bool compatibleMediaContainer =
            magic.has_value() && (kind == FormatKind::Audio || kind == FormatKind::Video) &&
            (magic->kind == FormatKind::Audio || magic->kind == FormatKind::Video);
        if (magic.has_value() && magic->kind != kind && !compatibleMediaContainer &&
            kind != FormatKind::Document && kind != FormatKind::ModelArtifact) {
            markMismatch(asset, magic->format, magic->mime);
        }
        return asset;
    }
    if (magic.has_value()) {
        applyDetectedMetadata(asset, *magic);
        asset.auditable = magic->kind == FormatKind::Pdf;
        asset.riskFlags.push_back("CONTENT_FORMAT_DETECTED");
        return asset;
    }
    if (textContent) {
        asset.format = "text";
        asset.mime = "text/plain";
        asset.auditable = true;
        asset.riskFlags.push_back("UNKNOWN_TEXT_EXTENSION");
    } else {
        asset.format = "binary";
        asset.mime = "application/octet-stream";
    }
    return asset;
}

ProjectAsset FormatDetector::detectMetadata(const std::filesystem::path& relativePath,
                                            std::uintmax_t sizeBytes) const {
    auto asset = metadataAsset(relativePath, sizeBytes);
    asset.auditable = false;
    return asset;
}

} // namespace cc
