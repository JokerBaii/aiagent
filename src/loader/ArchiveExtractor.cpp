/**
 * @file ArchiveExtractor.cpp
 * @brief 压缩包输入边界实现。
 */

#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/LibArchiveReader.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/loader/ZipArchiveReader.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <span>
#include <string_view>

namespace cc {
namespace {

constexpr std::size_t kArchiveProbeBytes = 512U;

[[nodiscard]] bool startsWith(std::span<const unsigned char> header,
                              std::initializer_list<unsigned char> prefix) {
    return header.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), header.begin());
}

[[nodiscard]] bool startsWithAt(std::span<const unsigned char> header, std::size_t offset,
                                std::initializer_list<unsigned char> prefix) {
    return offset <= header.size() && prefix.size() <= header.size() - offset &&
           std::equal(prefix.begin(), prefix.end(),
                      header.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] ArchiveFormat extensionFormat(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    const auto filename = util::lowerAscii(path.filename().string());
    if (extension == ".zip" || extension == ".apk" || extension == ".jar" || extension == ".war" ||
        extension == ".ear" || extension == ".whl" || extension == ".nupkg" ||
        extension == ".xpi" || extension == ".vsix" || extension == ".ipa" || extension == ".aab" ||
        extension == ".aar" || extension == ".egg") {
        return ArchiveFormat::Zip;
    }
    if (extension == ".gz" || extension == ".tgz" || filename.ends_with(".tar.gz")) {
        return ArchiveFormat::Gzip;
    }
    if (extension == ".7z") {
        return ArchiveFormat::SevenZip;
    }
    if (extension == ".xz" || filename.ends_with(".tar.xz")) {
        return ArchiveFormat::Xz;
    }
    if (extension == ".bz2" || filename.ends_with(".tar.bz2")) {
        return ArchiveFormat::Bzip2;
    }
    if (extension == ".zst" || filename.ends_with(".tar.zst")) {
        return ArchiveFormat::Zstd;
    }
    if (extension == ".rar") {
        return ArchiveFormat::Rar;
    }
    if (extension == ".tar") {
        return ArchiveFormat::Tar;
    }
    if (extension == ".cpio") {
        return ArchiveFormat::Cpio;
    }
    if (extension == ".ar" || extension == ".deb") {
        return ArchiveFormat::Ar;
    }
    if (extension == ".cab") {
        return ArchiveFormat::Cab;
    }
    if (extension == ".lz4") {
        return ArchiveFormat::Lz4;
    }
    if (extension == ".iso" || extension == ".rpm") {
        return ArchiveFormat::Unknown;
    }
    return ArchiveFormat::Unknown;
}

[[nodiscard]] bool hasArchiveExtension(const std::filesystem::path& path) {
    if (extensionFormat(path) != ArchiveFormat::Unknown) {
        return true;
    }
    const auto extension = util::lowerAscii(path.extension().string());
    return extension == ".iso" || extension == ".rpm";
}

[[nodiscard]] Result<ArchiveExtractionOutcome>
extractFiles(const std::filesystem::path& archivePath, const std::filesystem::path& inputRoot,
             const ImportLimits& limits, const ArchiveProbe& probe) {
    if (probe.reader == ArchiveReaderKind::NativeZip) {
        return ZipArchiveReader{}.extractAll(
            {.archivePath = archivePath, .destinationRoot = inputRoot, .limits = limits});
    }
    return LibArchiveReader{}.extractAll(
        {.archivePath = archivePath, .destinationRoot = inputRoot, .limits = limits});
}

} // namespace

Result<ProjectContext> ArchiveExtractor::extract(const ArchiveImportRequest& request) const {
    const auto& archivePath = request.archivePath;
    const auto& workspaceRoot = request.workspaceRoot;
    const auto detected = probe(archivePath);
    if (!detected.ok()) {
        return Result<ProjectContext>::failure(detected.error());
    }
    if (!detected.value().archive || detected.value().reader == ArchiveReaderKind::MetadataOnly ||
        detected.value().reader == ArchiveReaderKind::None) {
        return Result<ProjectContext>::failure("当前版本不支持该压缩格式: " +
                                               util::pathString(archivePath));
    }
    if (!detected.value().signatureMatched) {
        return Result<ProjectContext>::failure(
            "文件扩展名表示受支持的压缩包，但文件头不匹配，文件可能已损坏或扩展名错误: " +
            util::pathString(archivePath));
    }
    if (!std::filesystem::exists(archivePath) || !std::filesystem::is_regular_file(archivePath)) {
        return Result<ProjectContext>::failure("压缩包不存在或不可读: " +
                                               util::pathString(archivePath));
    }
    const auto normalized = PathGuard::normalize(archivePath);
    if (!normalized.ok()) {
        return Result<ProjectContext>::failure(normalized.error());
    }

    const auto inputRoot = workspaceRoot / "input";
    std::error_code ec;
    std::filesystem::create_directories(inputRoot, ec);
    if (ec) {
        return Result<ProjectContext>::failure("无法创建解包目录: " + ec.message());
    }
    auto extracted = extractFiles(archivePath, inputRoot, request.limits, detected.value());
    if (!extracted.ok()) {
        return Result<ProjectContext>::failure(extracted.error());
    }

    ProjectContext context;
    context.originalRoot = normalized.value();
    context.inputRoot = inputRoot;
    context.workspaceRoot = workspaceRoot;
    context.sessionId = workspaceRoot.filename().string();
    context.projectName = archivePath.stem().string();
    context.unpackStatus =
        detected.value().format == ArchiveFormat::Zip ? "ZIP_EXTRACTED" : "ARCHIVE_EXTRACTED";
    context.archiveInput = true;
    context.inputFiles = std::move(extracted.value().files);
    context.deferredFiles = std::move(extracted.value().deferredFiles);
    context.warnings = std::move(extracted.value().warnings);
    return Result<ProjectContext>::success(context);
}

ArchiveProbe ArchiveExtractor::probeHeader(std::span<const unsigned char> header) {
    ArchiveFormat format = ArchiveFormat::Unknown;
    if (startsWith(header, {'P', 'K', 0x03U, 0x04U}) ||
        startsWith(header, {'P', 'K', 0x05U, 0x06U}) ||
        startsWith(header, {'P', 'K', 0x07U, 0x08U})) {
        format = ArchiveFormat::Zip;
    } else if (startsWith(header, {0x1fU, 0x8bU})) {
        format = ArchiveFormat::Gzip;
    } else if (startsWith(header, {'7', 'z', 0xbcU, 0xafU, 0x27U, 0x1cU})) {
        format = ArchiveFormat::SevenZip;
    } else if (startsWith(header, {0xfdU, '7', 'z', 'X', 'Z', 0x00U})) {
        format = ArchiveFormat::Xz;
    } else if (header.size() >= 4U && startsWith(header, {'B', 'Z', 'h'}) && header[3] >= '1' &&
               header[3] <= '9') {
        format = ArchiveFormat::Bzip2;
    } else if (startsWith(header, {0x28U, 0xb5U, 0x2fU, 0xfdU})) {
        format = ArchiveFormat::Zstd;
    } else if (startsWith(header, {'R', 'a', 'r', '!', 0x1aU, 0x07U, 0x00U}) ||
               startsWith(header, {'R', 'a', 'r', '!', 0x1aU, 0x07U, 0x01U, 0x00U})) {
        format = ArchiveFormat::Rar;
    } else if (startsWithAt(header, 257U, {'u', 's', 't', 'a', 'r', 0x00U}) ||
               startsWithAt(header, 257U, {'u', 's', 't', 'a', 'r', ' '})) {
        format = ArchiveFormat::Tar;
    } else if (startsWith(header, {'0', '7', '0', '7', '0', '1'}) ||
               startsWith(header, {'0', '7', '0', '7', '0', '2'}) ||
               startsWith(header, {'0', '7', '0', '7', '0', '7'}) ||
               startsWith(header, {0x71U, 0xc7U}) || startsWith(header, {0xc7U, 0x71U})) {
        format = ArchiveFormat::Cpio;
    } else if (startsWith(header, {'!', '<', 'a', 'r', 'c', 'h', '>', '\n'})) {
        format = ArchiveFormat::Ar;
    } else if (startsWith(header, {'M', 'S', 'C', 'F'})) {
        format = ArchiveFormat::Cab;
    } else if (startsWith(header, {0x04U, 0x22U, 0x4dU, 0x18U})) {
        format = ArchiveFormat::Lz4;
    }

    if (format == ArchiveFormat::Unknown) {
        return {};
    }
    return {.archive = true,
            .signatureMatched = true,
            .format = format,
            .reader = format == ArchiveFormat::Zip ? ArchiveReaderKind::NativeZip
                                                   : ArchiveReaderKind::LibArchive};
}

Result<ArchiveProbe> ArchiveExtractor::probe(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<ArchiveProbe>::failure("无法读取文件头以识别归档格式: " +
                                             util::pathString(path));
    }
    std::array<unsigned char, kArchiveProbeBytes> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.bad()) {
        return Result<ArchiveProbe>::failure("读取文件头时发生错误: " + util::pathString(path));
    }
    const auto count = static_cast<std::size_t>(std::max<std::streamsize>(input.gcount(), 0));
    auto detected = probeHeader(std::span<const unsigned char>{bytes.data(), count});
    if (detected.archive) {
        return Result<ArchiveProbe>::success(detected);
    }
    if (!hasArchiveExtension(path)) {
        return Result<ArchiveProbe>::success({});
    }

    const auto format = extensionFormat(path);
    const auto supported = isSupportedArchivePath(path);
    return Result<ArchiveProbe>::success(
        {.archive = true,
         .signatureMatched = false,
         .format = format,
         .reader = supported ? ArchiveReaderKind::LibArchive : ArchiveReaderKind::MetadataOnly});
}

std::string ArchiveExtractor::formatName(ArchiveFormat format) {
    switch (format) {
    case ArchiveFormat::Zip:
        return "zip";
    case ArchiveFormat::Gzip:
        return "gzip";
    case ArchiveFormat::SevenZip:
        return "7z";
    case ArchiveFormat::Xz:
        return "xz";
    case ArchiveFormat::Bzip2:
        return "bzip2";
    case ArchiveFormat::Zstd:
        return "zstd";
    case ArchiveFormat::Rar:
        return "rar";
    case ArchiveFormat::Tar:
        return "tar";
    case ArchiveFormat::Cpio:
        return "cpio";
    case ArchiveFormat::Ar:
        return "ar";
    case ArchiveFormat::Cab:
        return "cab";
    case ArchiveFormat::Lz4:
        return "lz4";
    case ArchiveFormat::Unknown:
        return "archive";
    }
    return "archive";
}

bool ArchiveExtractor::isArchivePath(const std::filesystem::path& path) {
    return hasArchiveExtension(path);
}

bool ArchiveExtractor::isSupportedArchivePath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    const auto filename = util::lowerAscii(path.filename().string());
    return extension == ".zip" || extension == ".tar" || extension == ".gz" ||
           extension == ".tgz" || extension == ".7z" || extension == ".bz2" || extension == ".xz" ||
           extension == ".zst" || extension == ".cpio" || extension == ".ar" ||
           extension == ".deb" || extension == ".apk" || extension == ".jar" ||
           extension == ".war" || extension == ".ear" || extension == ".whl" ||
           extension == ".nupkg" || extension == ".xpi" || extension == ".vsix" ||
           extension == ".ipa" || extension == ".aab" || extension == ".aar" ||
           extension == ".egg" || filename.ends_with(".tar.gz") || filename.ends_with(".tar.bz2") ||
           filename.ends_with(".tar.xz") || filename.ends_with(".tar.zst");
}

} // namespace cc
