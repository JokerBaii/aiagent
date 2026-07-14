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

// 64 KiB is still a bounded probe, but reaches signatures stored away from byte zero
// (notably ISO-9660's volume descriptor at 0x8001) and enough ZIP entry names to
// distinguish common document containers.
constexpr std::size_t kFormatSampleBytes = 64U * 1024U;

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

[[nodiscard]] bool extensionIn(std::string_view extension,
                               std::initializer_list<std::string_view> values) {
    const auto lower = util::lowerAscii(std::string{extension});
    return std::find(values.begin(), values.end(), lower) != values.end();
}

[[nodiscard]] bool isKnownTextFileName(const std::filesystem::path& path) {
    const auto name = util::lowerAscii(path.filename().string());
    return extensionIn(name,
                       {"dockerfile", "containerfile", "makefile", "gnumakefile", "gemfile",
                        "rakefile",   "procfile",      "justfile", "vagrantfile", "brewfile",
                        "license",    "copying",       "notice",   "authors",     "contributors",
                        "readme",     "changelog",     "changes",  "install",     "manifest.in",
                        "jenkinsfile"});
}

[[nodiscard]] std::string formatFromExtension(const std::string& extension, std::string fallback) {
    return extension.empty() ? std::move(fallback) : extension.substr(1U);
}

[[nodiscard]] bool fileNameEndsWith(const std::filesystem::path& path, std::string_view suffix) {
    return util::lowerAscii(path.filename().string()).ends_with(suffix);
}

[[nodiscard]] FormatMetadata archiveMetadataForPath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    if (extensionIn(extension,
                    {".zip", ".whl", ".nupkg", ".xpi", ".vsix", ".ipa", ".aab", ".aar", ".egg"})) {
        return {FormatKind::Archive,
                extension == ".zip" ? "zip" : extension.substr(1U),
                "application/zip",
                {}};
    }
    if (extension == ".apk") {
        return {FormatKind::Archive, "apk", "application/vnd.android.package-archive", {}};
    }
    if (extensionIn(extension, {".jar", ".war", ".ear"})) {
        return {FormatKind::Archive, extension.substr(1U), "application/java-archive", {}};
    }
    if (extension == ".rar") {
        return {FormatKind::Archive, "rar", "application/vnd.rar", {}};
    }
    if (extension == ".7z") {
        return {FormatKind::Archive, "7z", "application/x-7z-compressed", {}};
    }
    if (extension == ".tar") {
        return {FormatKind::Archive, "tar", "application/x-tar", {}};
    }
    if (extension == ".tgz" || extension == ".gz") {
        return {FormatKind::Archive,
                fileNameEndsWith(path, ".tar.gz") || extension == ".tgz" ? "tar.gz" : "gzip",
                "application/gzip",
                {}};
    }
    if (extension == ".bz2") {
        return {FormatKind::Archive,
                fileNameEndsWith(path, ".tar.bz2") ? "tar.bz2" : "bzip2",
                "application/x-bzip2",
                {}};
    }
    if (extension == ".xz") {
        return {FormatKind::Archive,
                fileNameEndsWith(path, ".tar.xz") ? "tar.xz" : "xz",
                "application/x-xz",
                {}};
    }
    if (extension == ".zst") {
        return {FormatKind::Archive,
                fileNameEndsWith(path, ".tar.zst") ? "tar.zst" : "zstd",
                "application/zstd",
                {}};
    }
    if (extension == ".cpio") {
        return {FormatKind::Archive, "cpio", "application/x-cpio", {}};
    }
    if (extension == ".ar") {
        return {FormatKind::Archive, "ar", "application/x-archive", {}};
    }
    if (extension == ".deb") {
        return {FormatKind::Archive, "deb", "application/vnd.debian.binary-package", {}};
    }
    if (extension == ".cab") {
        return {FormatKind::Archive, "cab", "application/vnd.ms-cab-compressed", {}};
    }
    if (extension == ".lz4") {
        return {FormatKind::Archive, "lz4", "application/x-lz4", {}};
    }
    if (extension == ".iso") {
        return {FormatKind::Archive, "iso", "application/x-iso9660-image", {}};
    }
    if (extension == ".rpm") {
        return {FormatKind::Archive, "rpm", "application/x-rpm", {}};
    }
    return {FormatKind::Archive, "archive", "application/octet-stream", {}};
}

[[nodiscard]] FormatMetadata officeMetadata(std::string_view extension) {
    if (extensionIn(extension, {".docx", ".docm", ".dotx", ".dotm"})) {
        return {FormatKind::OfficeOpenXml,
                std::string{extension.substr(1U)},
                extension == ".docm"   ? "application/vnd.ms-word.document.macroenabled.12"
                : extension == ".dotm" ? "application/vnd.ms-word.template.macroenabled.12"
                : extension == ".dotx"
                    ? "application/vnd.openxmlformats-officedocument.wordprocessingml.template"
                    : "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                {}};
    }
    if (extensionIn(extension, {".xlsx", ".xlsm", ".xltx", ".xltm", ".xlsb"})) {
        return {FormatKind::OfficeOpenXml,
                std::string{extension.substr(1U)},
                extension == ".xlsb"   ? "application/vnd.ms-excel.sheet.binary.macroenabled.12"
                : extension == ".xlsm" ? "application/vnd.ms-excel.sheet.macroenabled.12"
                : extension == ".xltm" ? "application/vnd.ms-excel.template.macroenabled.12"
                : extension == ".xltx"
                    ? "application/vnd.openxmlformats-officedocument.spreadsheetml.template"
                    : "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                {}};
    }
    if (extension == ".vsdx") {
        return {FormatKind::OfficeOpenXml, "vsdx", "application/vnd.ms-visio.drawing", {}};
    }
    return {FormatKind::OfficeOpenXml,
            std::string{extension.substr(1U)},
            extension == ".pptm"   ? "application/vnd.ms-powerpoint.presentation.macroenabled.12"
            : extension == ".potm" ? "application/vnd.ms-powerpoint.template.macroenabled.12"
            : extension == ".ppsm" ? "application/vnd.ms-powerpoint.slideshow.macroenabled.12"
            : extension == ".potx"
                ? "application/vnd.openxmlformats-officedocument.presentationml.template"
            : extension == ".ppsx"
                ? "application/vnd.openxmlformats-officedocument.presentationml.slideshow"
            : extensionIn(extension, {".sldx", ".sldm"})
                ? "application/vnd.openxmlformats-officedocument.presentationml.slide"
                : "application/vnd.openxmlformats-officedocument.presentationml.presentation",
            {}};
}

[[nodiscard]] FormatMetadata metadataForPath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    if (ArchiveExtractor::isArchivePath(path)) {
        return archiveMetadataForPath(path);
    }
    if (isCodeExtension(extension)) {
        return {FormatKind::Code, formatFromExtension(extension, "source"), "text/x-source-code",
                formatFromExtension(extension, "source")};
    }
    if (isLikelyTextExtension(extension) || isKnownTextFileName(path)) {
        return {FormatKind::Text, formatFromExtension(extension, "text"), "text/plain", {}};
    }
    if (isOfficeExtension(extension)) {
        return officeMetadata(extension);
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
    if (extensionIn(extension, {".odt", ".ott", ".ods", ".ots", ".odp", ".otp", ".odg"})) {
        const auto type = extensionIn(extension, {".odt", ".ott"})   ? "text"
                          : extensionIn(extension, {".ods", ".ots"}) ? "spreadsheet"
                          : extensionIn(extension, {".odp", ".otp"}) ? "presentation"
                                                                     : "graphics";
        return {FormatKind::Document,
                extension.substr(1U),
                "application/vnd.oasis.opendocument." + std::string{type},
                {}};
    }
    if (extensionIn(extension, {".epub", ".mobi", ".azw", ".azw3", ".fb2", ".chm"})) {
        const auto mime = extension == ".epub"   ? "application/epub+zip"
                          : extension == ".mobi" ? "application/x-mobipocket-ebook"
                          : extension == ".fb2"  ? "application/x-fictionbook+xml"
                          : extension == ".chm"  ? "application/vnd.ms-htmlhelp"
                                                 : "application/vnd.amazon.ebook";
        return {FormatKind::Document, extension.substr(1U), mime, {}};
    }
    if (extensionIn(extension,
                    {".png",  ".apng", ".jpg",  ".jpeg", ".jpe", ".gif", ".webp", ".svg",
                     ".bmp",  ".dib",  ".tiff", ".tif",  ".ico", ".cur", ".avif", ".heic",
                     ".heif", ".jxl",  ".jp2",  ".j2k",  ".psd", ".ai",  ".eps",  ".dds",
                     ".exr",  ".hdr",  ".icns", ".pbm",  ".pgm", ".ppm", ".pnm",  ".dng",
                     ".raw",  ".cr2",  ".cr3",  ".nef",  ".arw", ".orf", ".rw2"})) {
        const auto format = extension.substr(1U);
        const auto mime = extensionIn(extension, {".jpg", ".jpeg", ".jpe"}) ? "image/jpeg"
                          : extensionIn(extension, {".tif", ".tiff"})       ? "image/tiff"
                          : extension == ".svg"                             ? "image/svg+xml"
                          : extension == ".ico" || extension == ".cur"      ? "image/x-icon"
                          : extension == ".psd" ? "image/vnd.adobe.photoshop"
                                                : "image/" + format;
        return {FormatKind::Image, format, mime, {}};
    }
    if (extensionIn(extension, {".mp4", ".mov", ".avi", ".mkv", ".webm", ".mpeg", ".mpg",
                                ".m4v", ".wmv", ".flv", ".mts", ".m2ts", ".ts",   ".vob",
                                ".ogv", ".3gp", ".3g2", ".rm",  ".rmvb", ".asf"})) {
        const auto format = extension.substr(1U);
        const auto mime = extensionIn(extension, {".mpeg", ".mpg"}) ? "video/mpeg"
                          : extension == ".mkv"                     ? "video/x-matroska"
                          : extension == ".mov"                     ? "video/quicktime"
                          : extension == ".3gp"                     ? "video/3gpp"
                          : extension == ".3g2"                     ? "video/3gpp2"
                                                                    : "video/" + format;
        return {FormatKind::Video, format, mime, {}};
    }
    if (extensionIn(extension,
                    {".mp3", ".wav", ".flac", ".ogg", ".oga", ".m4a", ".aac", ".opus", ".wma",
                     ".aiff", ".aif", ".alac", ".amr", ".ape", ".mid", ".midi"})) {
        const auto format = extension.substr(1U);
        const auto mime = extension == ".mp3"                         ? "audio/mpeg"
                          : extensionIn(extension, {".mid", ".midi"}) ? "audio/midi"
                          : extensionIn(extension, {".aif", ".aiff"}) ? "audio/aiff"
                                                                      : "audio/" + format;
        return {FormatKind::Audio, format, mime, {}};
    }
    if (extensionIn(extension,
                    {".onnx",        ".pt",      ".pth",        ".pkl",     ".pickle", ".joblib",
                     ".safetensors", ".tflite",  ".pb",         ".ckpt",    ".gguf",   ".ggml",
                     ".keras",       ".h5",      ".caffemodel", ".mlmodel", ".engine", ".trt",
                     ".weights",     ".pdmodel", ".pdiparams"})) {
        return {
            FormatKind::ModelArtifact, extension.substr(1U), "application/x-model-artifact", {}};
    }
    if (extensionIn(extension,
                    {".glb",  ".gltf", ".fbx",  ".obj",  ".stl", ".dae", ".3ds",  ".blend",
                     ".usd",  ".usda", ".usdc", ".usdz", ".ply", ".3mf", ".step", ".stp",
                     ".iges", ".igs",  ".dwg",  ".dxf",  ".ifc", ".skp", ".mtl"})) {
        const auto format = extension.substr(1U);
        return {FormatKind::ThreeDimensionalModel, format, "model/" + format, {}};
    }
    if (extensionIn(extension,
                    {".parquet", ".avro",  ".orc",  ".feather", ".arrow",   ".npy", ".npz",
                     ".mat",     ".hdf",   ".hdf5", ".sqlite",  ".sqlite3", ".db3", ".duckdb",
                     ".mdb",     ".accdb", ".dbf",  ".shp",     ".shx",     ".las", ".laz"})) {
        const auto mime = extensionIn(extension, {".sqlite", ".sqlite3", ".db3"})
                              ? "application/x-sqlite3"
                          : extension == ".parquet" ? "application/vnd.apache.parquet"
                          : extension == ".avro"    ? "application/avro"
                          : extension == ".npy"     ? "application/x-numpy"
                          : extension == ".hdf" || extension == ".hdf5" || extension == ".h5"
                              ? "application/x-hdf5"
                              : "application/x-data-artifact";
        return {FormatKind::Binary, extension.substr(1U), mime, {}};
    }
    if (extensionIn(extension, {".ttf", ".otf", ".woff", ".woff2", ".eot"})) {
        return {FormatKind::Binary, extension.substr(1U), "font/" + extension.substr(1U), {}};
    }
    if (extensionIn(extension,
                    {".exe", ".dll", ".so",  ".dylib", ".a",   ".o",   ".class", ".wasm",
                     ".bin", ".dat", ".db",  ".dmg",   ".msi", ".com", ".scr",   ".appimage",
                     ".pyc", ".pyo", ".dex", ".ko",    ".lib", ".pdb", ".crx"})) {
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

[[nodiscard]] bool containsBytes(const std::vector<unsigned char>& sample,
                                 std::string_view needle) {
    if (needle.empty() || sample.size() < needle.size()) {
        return false;
    }
    const auto bytes =
        std::string_view{reinterpret_cast<const char*>(sample.data()), sample.size()};
    return bytes.find(needle) != std::string_view::npos;
}

[[nodiscard]] FormatMetadata zipContainerMetadata(const std::vector<unsigned char>& sample) {
    if (containsBytes(sample, "word/")) {
        return {FormatKind::OfficeOpenXml,
                "docx",
                "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                {}};
    }
    if (containsBytes(sample, "ppt/")) {
        return {FormatKind::OfficeOpenXml,
                "pptx",
                "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                {}};
    }
    if (containsBytes(sample, "xl/")) {
        return {FormatKind::OfficeOpenXml,
                "xlsx",
                "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                {}};
    }
    if (containsBytes(sample, "application/epub+zip")) {
        return {FormatKind::Document, "epub", "application/epub+zip", {}};
    }
    if (containsBytes(sample, "application/vnd.oasis.opendocument.text")) {
        return {FormatKind::Document, "odt", "application/vnd.oasis.opendocument.text", {}};
    }
    if (containsBytes(sample, "application/vnd.oasis.opendocument.spreadsheet")) {
        return {FormatKind::Document, "ods", "application/vnd.oasis.opendocument.spreadsheet", {}};
    }
    if (containsBytes(sample, "application/vnd.oasis.opendocument.presentation")) {
        return {FormatKind::Document, "odp", "application/vnd.oasis.opendocument.presentation", {}};
    }
    if (containsBytes(sample, "AndroidManifest.xml")) {
        return {FormatKind::Archive, "apk", "application/vnd.android.package-archive", {}};
    }
    return {FormatKind::Archive, "zip", "application/zip", {}};
}

[[nodiscard]] std::optional<FormatMetadata>
metadataForMagic(const std::vector<unsigned char>& sample) {
    if (isPdf(sample)) {
        return FormatMetadata{FormatKind::Pdf, "pdf", "application/pdf", {}};
    }
    if (isZip(sample)) {
        return zipContainerMetadata(sample);
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
    if (startsWith(sample, {'B', 'M'})) {
        return FormatMetadata{FormatKind::Image, "bmp", "image/bmp", {}};
    }
    if (startsWith(sample, {'I', 'I', 0x2aU, 0x00U}) ||
        startsWith(sample, {'M', 'M', 0x00U, 0x2aU})) {
        return FormatMetadata{FormatKind::Image, "tiff", "image/tiff", {}};
    }
    if (startsWith(sample, {0x00U, 0x00U, 0x01U, 0x00U})) {
        return FormatMetadata{FormatKind::Image, "ico", "image/x-icon", {}};
    }
    if (startsWith(sample, {'8', 'B', 'P', 'S'})) {
        return FormatMetadata{FormatKind::Image, "psd", "image/vnd.adobe.photoshop", {}};
    }
    if (startsWith(sample,
                   {0x00U, 0x00U, 0x00U, 0x0cU, 'j', 'P', ' ', ' ', 0x0dU, 0x0aU, 0x87U, 0x0aU})) {
        return FormatMetadata{FormatKind::Image, "jp2", "image/jp2", {}};
    }
    if (startsWith(sample, {'D', 'D', 'S', ' '})) {
        return FormatMetadata{FormatKind::Image, "dds", "image/vnd-ms.dds", {}};
    }
    if (startsWith(sample, {0x76U, 0x2fU, 0x31U, 0x01U})) {
        return FormatMetadata{FormatKind::Image, "exr", "image/x-exr", {}};
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
    if (sample.size() >= 2U && sample[0] == 0xffU && (sample[1] & 0xe0U) == 0xe0U) {
        return FormatMetadata{FormatKind::Audio, "mp3", "audio/mpeg", {}};
    }
    if (startsWith(sample, {'F', 'O', 'R', 'M'}) &&
        (startsWithAt(sample, 8U, {'A', 'I', 'F', 'F'}) ||
         startsWithAt(sample, 8U, {'A', 'I', 'F', 'C'}))) {
        return FormatMetadata{FormatKind::Audio, "aiff", "audio/aiff", {}};
    }
    if (startsWith(sample, {'M', 'T', 'h', 'd'})) {
        return FormatMetadata{FormatKind::Audio, "midi", "audio/midi", {}};
    }
    if (startsWith(sample, {'R', 'I', 'F', 'F'}) &&
        startsWithAt(sample, 8U, {'A', 'V', 'I', ' '})) {
        return FormatMetadata{FormatKind::Video, "avi", "video/x-msvideo", {}};
    }
    if (startsWith(sample, {'F', 'L', 'V'})) {
        return FormatMetadata{FormatKind::Video, "flv", "video/x-flv", {}};
    }
    if (startsWith(sample, {0x1aU, 0x45U, 0xdfU, 0xa3U})) {
        return containsBytes(sample, "webm")
                   ? FormatMetadata{FormatKind::Video, "webm", "video/webm", {}}
                   : FormatMetadata{FormatKind::Video, "mkv", "video/x-matroska", {}};
    }
    if (startsWithAt(sample, 4U, {'f', 't', 'y', 'p'})) {
        if (startsWithAt(sample, 8U, {'a', 'v', 'i', 'f'}) ||
            startsWithAt(sample, 8U, {'a', 'v', 'i', 's'})) {
            return FormatMetadata{FormatKind::Image, "avif", "image/avif", {}};
        }
        if (startsWithAt(sample, 8U, {'h', 'e', 'i', 'c'}) ||
            startsWithAt(sample, 8U, {'h', 'e', 'i', 'x'}) ||
            startsWithAt(sample, 8U, {'m', 'i', 'f', '1'})) {
            return FormatMetadata{FormatKind::Image, "heic", "image/heic", {}};
        }
        if (startsWithAt(sample, 8U, {'j', 'x', 'l', ' '})) {
            return FormatMetadata{FormatKind::Image, "jxl", "image/jxl", {}};
        }
        return FormatMetadata{FormatKind::Video, "mp4", "video/mp4", {}};
    }
    if (startsWith(sample, {'g', 'l', 'T', 'F'})) {
        return FormatMetadata{FormatKind::ThreeDimensionalModel, "glb", "model/gltf-binary", {}};
    }
    if (startsWith(sample, {'B', 'L', 'E', 'N', 'D', 'E', 'R'})) {
        return FormatMetadata{FormatKind::ThreeDimensionalModel, "blend", "model/vnd.blender", {}};
    }
    if (startsWithAt(sample, 4U, {'T', 'F', 'L', '3'})) {
        return FormatMetadata{
            FormatKind::ModelArtifact, "tflite", "application/x-model-artifact", {}};
    }
    if (startsWith(sample, {0x1fU, 0x8bU})) {
        return FormatMetadata{FormatKind::Archive, "gzip", "application/gzip", {}};
    }
    if (startsWith(sample, {'7', 'z', 0xbcU, 0xafU, 0x27U, 0x1cU})) {
        return FormatMetadata{FormatKind::Archive, "7z", "application/x-7z-compressed", {}};
    }
    if (startsWith(sample, {0x28U, 0xb5U, 0x2fU, 0xfdU})) {
        return FormatMetadata{FormatKind::Archive, "zstd", "application/zstd", {}};
    }
    if (startsWith(sample, {0xfdU, '7', 'z', 'X', 'Z', 0x00U})) {
        return FormatMetadata{FormatKind::Archive, "xz", "application/x-xz", {}};
    }
    if (sample.size() >= 4U && startsWith(sample, {'B', 'Z', 'h'}) && sample[3] >= '1' &&
        sample[3] <= '9') {
        return FormatMetadata{FormatKind::Archive, "bzip2", "application/x-bzip2", {}};
    }
    if (startsWith(sample, {'R', 'a', 'r', '!', 0x1aU, 0x07U, 0x00U}) ||
        startsWith(sample, {'R', 'a', 'r', '!', 0x1aU, 0x07U, 0x01U, 0x00U})) {
        return FormatMetadata{FormatKind::Archive, "rar", "application/vnd.rar", {}};
    }
    if (startsWithAt(sample, 257U, {'u', 's', 't', 'a', 'r', 0x00U}) ||
        startsWithAt(sample, 257U, {'u', 's', 't', 'a', 'r', ' '})) {
        return FormatMetadata{FormatKind::Archive, "tar", "application/x-tar", {}};
    }
    if (startsWith(sample, {'!', '<', 'a', 'r', 'c', 'h', '>', '\n'})) {
        return FormatMetadata{FormatKind::Archive, "ar", "application/x-archive", {}};
    }
    if (startsWith(sample, {'M', 'S', 'C', 'F'})) {
        return FormatMetadata{FormatKind::Archive, "cab", "application/vnd.ms-cab-compressed", {}};
    }
    if (startsWith(sample, {0x04U, 0x22U, 0x4dU, 0x18U})) {
        return FormatMetadata{FormatKind::Archive, "lz4", "application/x-lz4", {}};
    }
    if (startsWithAt(sample, 0x8001U, {'C', 'D', '0', '0', '1'})) {
        return FormatMetadata{FormatKind::Archive, "iso", "application/x-iso9660-image", {}};
    }
    if (startsWith(sample, {'{', '\\', 'r', 't', 'f'})) {
        return FormatMetadata{FormatKind::Document, "rtf", "application/rtf", {}};
    }
    if (startsWith(sample, {0xd0U, 0xcfU, 0x11U, 0xe0U, 0xa1U, 0xb1U, 0x1aU, 0xe1U})) {
        return FormatMetadata{FormatKind::Document, "ole", "application/x-ole-storage", {}};
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
    if (startsWith(sample, {0xfeU, 0xedU, 0xfaU, 0xceU}) ||
        startsWith(sample, {0xceU, 0xfaU, 0xedU, 0xfeU}) ||
        startsWith(sample, {0xfeU, 0xedU, 0xfaU, 0xcfU}) ||
        startsWith(sample, {0xcfU, 0xfaU, 0xedU, 0xfeU})) {
        return FormatMetadata{FormatKind::Binary, "mach-o", "application/x-mach-binary", {}};
    }
    if (startsWith(sample, {0x00U, 'a', 's', 'm'})) {
        return FormatMetadata{FormatKind::Binary, "wasm", "application/wasm", {}};
    }
    if (startsWith(sample, {0xcaU, 0xfeU, 0xbaU, 0xbeU})) {
        return FormatMetadata{FormatKind::Binary, "class", "application/java-vm", {}};
    }
    if (startsWith(sample, {'d', 'e', 'x', '\n'})) {
        return FormatMetadata{FormatKind::Binary, "dex", "application/vnd.android.dex", {}};
    }
    if (startsWith(sample, {0x89U, 'H', 'D', 'F', '\r', '\n', 0x1aU, '\n'})) {
        return FormatMetadata{FormatKind::Binary, "hdf5", "application/x-hdf5", {}};
    }
    if (startsWith(sample, {0x93U, 'N', 'U', 'M', 'P', 'Y'})) {
        return FormatMetadata{FormatKind::Binary, "npy", "application/x-numpy", {}};
    }
    if (startsWith(sample, {'P', 'A', 'R', '1'})) {
        return FormatMetadata{FormatKind::Binary, "parquet", "application/vnd.apache.parquet", {}};
    }
    if (startsWith(sample, {'O', 'b', 'j', 0x01U})) {
        return FormatMetadata{FormatKind::Binary, "avro", "application/avro", {}};
    }
    if (startsWith(sample, {'G', 'G', 'U', 'F'})) {
        return FormatMetadata{
            FormatKind::ModelArtifact, "gguf", "application/x-model-artifact", {}};
    }
    if (startsWith(sample, {'b', 'p', 'l', 'i', 's', 't', '0', '0'})) {
        return FormatMetadata{FormatKind::Binary, "binary-plist", "application/x-plist", {}};
    }
    if (startsWith(sample, {0x00U, 0x01U, 0x00U, 0x00U})) {
        return FormatMetadata{FormatKind::Binary, "ttf", "font/ttf", {}};
    }
    if (startsWith(sample, {'O', 'T', 'T', 'O'})) {
        return FormatMetadata{FormatKind::Binary, "otf", "font/otf", {}};
    }
    if (startsWith(sample, {'w', 'O', 'F', 'F'})) {
        return FormatMetadata{FormatKind::Binary, "woff", "font/woff", {}};
    }
    if (startsWith(sample, {'w', 'O', 'F', '2'})) {
        return FormatMetadata{FormatKind::Binary, "woff2", "font/woff2", {}};
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
    return extensionIn(
        extension,
        {".md",           ".markdown",  ".mdown",         ".mkd",        ".txt",     ".text",
         ".rst",          ".adoc",      ".asc",           ".org",        ".json",    ".jsonl",
         ".ndjson",       ".geojson",   ".json5",         ".jsonc",      ".hjson",   ".yaml",
         ".yml",          ".csv",       ".tsv",           ".psv",        ".xml",     ".xsd",
         ".xsl",          ".xslt",      ".xaml",          ".plist",      ".html",    ".htm",
         ".xhtml",        ".mhtml",     ".css",           ".scss",       ".sass",    ".less",
         ".styl",         ".toml",      ".ini",           ".cfg",        ".conf",    ".config",
         ".cnf",          ".env",       ".sql",           ".log",        ".diff",    ".patch",
         ".editorconfig", ".gitignore", ".gitattributes", ".gitmodules", ".ignore",  ".lock",
         ".properties",   ".graphql",   ".gql",           ".tex",        ".latex",   ".bib",
         ".ics",          ".vcf",       ".srt",           ".vtt",        ".ass",     ".ssa",
         ".po",           ".pot",       ".strings",       ".desktop",    ".service", ".socket",
         ".mount",        ".rules",     ".manifest",      ".mf",         ".spec",    ".lst",
         ".list",         ".map"});
}

bool isCodeExtension(const std::string& extension) {
    return extensionIn(
        extension,
        {".c",      ".cc",     ".cpp",    ".cxx",    ".h",    ".hh",     ".hpp",  ".hxx",
         ".m",      ".mm",     ".py",     ".pyw",    ".pyi",  ".pyx",    ".pxd",  ".ipynb",
         ".js",     ".mjs",    ".cjs",    ".ts",     ".mts",  ".cts",    ".tsx",  ".jsx",
         ".vue",    ".svelte", ".astro",  ".qml",    ".java", ".kt",     ".kts",  ".groovy",
         ".gvy",    ".go",     ".rs",     ".swift",  ".cs",   ".vb",     ".fs",   ".fsx",
         ".fsi",    ".php",    ".rb",     ".pl",     ".pm",   ".lua",    ".r",    ".jl",
         ".scala",  ".sc",     ".dart",   ".ex",     ".exs",  ".erl",    ".hrl",  ".clj",
         ".cljs",   ".cljc",   ".edn",    ".hs",     ".lhs",  ".elm",    ".ml",   ".mli",
         ".nim",    ".zig",    ".cr",     ".coffee", ".pas",  ".pp",     ".f",    ".f77",
         ".f90",    ".f95",    ".f03",    ".for",    ".cob",  ".cbl",    ".lisp", ".lsp",
         ".cl",     ".scm",    ".rkt",    ".tcl",    ".awk",  ".sed",    ".sh",   ".bash",
         ".zsh",    ".fish",   ".ps1",    ".psm1",   ".bat",  ".cmd",    ".sql",  ".cmake",
         ".gradle", ".proto",  ".thrift", ".sol",    ".move", ".asm",    ".s",    ".v",
         ".sv",     ".svh",    ".vhd",    ".vhdl",   ".tf",   ".tfvars", ".hcl",  ".nix",
         ".cue",    ".rego",   ".prisma", ".wgsl",   ".glsl", ".vert",   ".frag", ".geom",
         ".comp",   ".cu",     ".cuh",    ".mk",     ".mak"});
}

bool isOfficeExtension(const std::string& extension) {
    return extensionIn(extension, {".docx", ".docm", ".dotx", ".dotm", ".xlsx", ".xlsm", ".xltx",
                                   ".xltm", ".xlsb", ".pptx", ".pptm", ".potx", ".potm", ".ppsx",
                                   ".ppsm", ".sldx", ".sldm", ".vsdx"});
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
        if (magic.has_value() && magic->kind != FormatKind::Archive &&
            magic->kind != FormatKind::OfficeOpenXml && magic->kind != FormatKind::Document) {
            markMismatch(asset, magic->format, magic->mime);
        } else if (textContent && !zipContent) {
            markMismatch(asset, "text", "text/plain");
        }
        return asset;
    }
    if (kind == FormatKind::Image) {
        if (magic.has_value() && magic->kind != FormatKind::Image) {
            markMismatch(asset, magic->format, magic->mime);
        } else if (asset.extension != ".svg" && textContent) {
            markMismatch(asset, "text", "text/plain");
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
        } else if (kind == FormatKind::Binary && textContent) {
            markMismatch(asset, "text", "text/plain");
        }
        return asset;
    }
    if (magic.has_value()) {
        applyDetectedMetadata(asset, *magic);
        asset.auditable =
            magic->kind == FormatKind::Pdf || magic->kind == FormatKind::OfficeOpenXml;
        asset.riskFlags.push_back("CONTENT_FORMAT_DETECTED");
        return asset;
    }
    if (textContent) {
        if (asset.extension.empty()) {
            asset.format = "text";
        }
        asset.mime = "text/plain";
        asset.auditable = true;
        asset.riskFlags.push_back("UNKNOWN_TEXT_EXTENSION");
    } else {
        if (asset.extension.empty()) {
            asset.format = "binary";
        }
        asset.mime = "application/octet-stream";
        asset.riskFlags.push_back("UNRECOGNIZED_BINARY_FORMAT");
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
