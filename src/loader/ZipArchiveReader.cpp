/**
 * @file ZipArchiveReader.cpp
 * @brief ZIP 元数据校验和有预算的事务性解压。
 */

#include "cc/loader/ZipArchiveReader.hpp"

#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include <zlib.h>

namespace cc {
namespace {

constexpr std::uint32_t kEndOfCentralDirectory = 0x06054b50U;
constexpr std::uint32_t kCentralDirectoryHeader = 0x02014b50U;
constexpr std::uint32_t kLocalFileHeader = 0x04034b50U;
constexpr std::uint16_t kStoreMethod = 0U;
constexpr std::uint16_t kDeflateMethod = 8U;
constexpr std::uint16_t kEncryptedFlag = 0x0001U;
constexpr std::uint16_t kDataDescriptorFlag = 0x0008U;
constexpr std::size_t kEocdMinimumSize = 22U;
constexpr std::size_t kCentralHeaderSize = 46U;
constexpr std::size_t kLocalHeaderSize = 30U;

struct ParsedEntry {
    ZipArchiveEntry entry;
    std::uint32_t localHeaderOffset{0};
    std::uint32_t crc{0};
    std::uint16_t flags{0};
};

class ExtractionRollback {
  public:
    explicit ExtractionRollback(std::filesystem::path root) : root_{std::move(root)} {
        std::error_code error;
        rootExisted_ = std::filesystem::exists(root_, error);
    }

    ~ExtractionRollback() {
        if (!armed_ || committed_) {
            return;
        }
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
        if (rootExisted_) {
            std::filesystem::create_directories(root_, ignored);
        }
    }

    void arm() { armed_ = true; }
    void commit() { committed_ = true; }

  private:
    std::filesystem::path root_;
    bool rootExisted_{false};
    bool armed_{false};
    bool committed_{false};
};

[[nodiscard]] bool rangeInside(std::size_t offset, std::size_t length, std::size_t size) {
    return offset <= size && length <= size - offset;
}

[[nodiscard]] Result<std::vector<unsigned char>>
readBinaryFile(const std::filesystem::path& path, const ImportLimits& limits) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return Result<std::vector<unsigned char>>::failure("无法读取 ZIP 文件大小: " +
                                                           error.message());
    }
    if (size > limits.maxArchiveBytes ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        return Result<std::vector<unsigned char>>::failure("ZIP 文件超过导入读取上限");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::vector<unsigned char>>::failure("无法读取 ZIP 文件: " +
                                                           util::pathString(path));
    }
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
        if (input.gcount() != static_cast<std::streamsize>(data.size())) {
            return Result<std::vector<unsigned char>>::failure("ZIP 文件读取不完整");
        }
    }
    return Result<std::vector<unsigned char>>::success(std::move(data));
}

[[nodiscard]] std::uint16_t readU16(const std::vector<unsigned char>& data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t readU32(const std::vector<unsigned char>& data, std::size_t offset) {
    return static_cast<std::uint32_t>(readU16(data, offset)) |
           (static_cast<std::uint32_t>(readU16(data, offset + 2U)) << 16U);
}

[[nodiscard]] Result<std::size_t>
findEndOfCentralDirectory(const std::vector<unsigned char>& data) {
    if (data.size() < kEocdMinimumSize) {
        return Result<std::size_t>::failure("ZIP 文件过小或缺少中央目录");
    }
    const auto maximumComment = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max());
    const auto searchStart = data.size() > maximumComment + kEocdMinimumSize
                                 ? data.size() - maximumComment - kEocdMinimumSize
                                 : 0U;
    for (std::size_t offset = data.size() - kEocdMinimumSize + 1U; offset-- > searchStart;) {
        if (readU32(data, offset) == kEndOfCentralDirectory) {
            return Result<std::size_t>::success(offset);
        }
        if (offset == 0U) {
            break;
        }
    }
    return Result<std::size_t>::failure("ZIP 文件缺少中央目录结束记录");
}

[[nodiscard]] Result<std::vector<ParsedEntry>>
parseCentralDirectory(const std::vector<unsigned char>& data) {
    const auto eocd = findEndOfCentralDirectory(data);
    if (!eocd.ok()) {
        return Result<std::vector<ParsedEntry>>::failure(eocd.error());
    }
    if (!rangeInside(eocd.value(), kEocdMinimumSize, data.size())) {
        return Result<std::vector<ParsedEntry>>::failure("ZIP 中央目录结束记录损坏");
    }
    const auto commentLength = static_cast<std::size_t>(readU16(data, eocd.value() + 20U));
    if (!rangeInside(eocd.value() + kEocdMinimumSize, commentLength, data.size())) {
        return Result<std::vector<ParsedEntry>>::failure("ZIP 注释越过文件边界");
    }
    if (readU16(data, eocd.value() + 4U) != 0U || readU16(data, eocd.value() + 6U) != 0U) {
        return Result<std::vector<ParsedEntry>>::failure("不支持分卷 ZIP 文件");
    }
    const auto diskEntryCount = readU16(data, eocd.value() + 8U);
    const auto entryCount = readU16(data, eocd.value() + 10U);
    if (entryCount == std::numeric_limits<std::uint16_t>::max() || diskEntryCount != entryCount) {
        return Result<std::vector<ParsedEntry>>::failure("不支持 Zip64 或分卷 ZIP 目录");
    }
    const auto directorySize = static_cast<std::size_t>(readU32(data, eocd.value() + 12U));
    const auto directoryOffset = static_cast<std::size_t>(readU32(data, eocd.value() + 16U));
    if (!rangeInside(directoryOffset, directorySize, data.size()) ||
        directoryOffset + directorySize > eocd.value()) {
        return Result<std::vector<ParsedEntry>>::failure("ZIP 中央目录越过文件边界");
    }

    std::vector<ParsedEntry> entries;
    entries.reserve(entryCount);
    std::size_t offset = directoryOffset;
    for (std::uint16_t index = 0; index < entryCount; ++index) {
        if (!rangeInside(offset, kCentralHeaderSize, data.size()) ||
            readU32(data, offset) != kCentralDirectoryHeader) {
            return Result<std::vector<ParsedEntry>>::failure("ZIP 中央目录条目损坏");
        }
        const auto fileNameLength = static_cast<std::size_t>(readU16(data, offset + 28U));
        const auto extraLength = static_cast<std::size_t>(readU16(data, offset + 30U));
        const auto entryCommentLength = static_cast<std::size_t>(readU16(data, offset + 32U));
        const auto variableLength = fileNameLength + extraLength + entryCommentLength;
        if (!rangeInside(offset + kCentralHeaderSize, variableLength, data.size())) {
            return Result<std::vector<ParsedEntry>>::failure("ZIP 条目元数据越过文件边界");
        }
        const auto nameOffset = offset + kCentralHeaderSize;
        std::string name{reinterpret_cast<const char*>(data.data() + nameOffset), fileNameLength};
        if (name.find('\0') != std::string::npos) {
            return Result<std::vector<ParsedEntry>>::failure("ZIP 条目名称包含空字符");
        }

        const auto compressedSize = readU32(data, offset + 20U);
        const auto uncompressedSize = readU32(data, offset + 24U);
        if (compressedSize == std::numeric_limits<std::uint32_t>::max() ||
            uncompressedSize == std::numeric_limits<std::uint32_t>::max()) {
            return Result<std::vector<ParsedEntry>>::failure("当前版本不支持 Zip64 条目: " + name);
        }

        ParsedEntry parsed;
        parsed.entry.relativePath = std::filesystem::path{name};
        parsed.entry.directory = !name.empty() && name.back() == '/';
        const auto externalAttributes = readU32(data, offset + 38U);
        parsed.entry.symlink = ((externalAttributes >> 16U) & 0170000U) == 0120000U;
        parsed.entry.compressionMethod = readU16(data, offset + 10U);
        parsed.entry.compressedSize = compressedSize;
        parsed.entry.uncompressedSize = uncompressedSize;
        parsed.localHeaderOffset = readU32(data, offset + 42U);
        parsed.crc = readU32(data, offset + 16U);
        parsed.flags = readU16(data, offset + 8U);
        entries.push_back(std::move(parsed));
        offset += kCentralHeaderSize + variableLength;
    }
    if (offset != directoryOffset + directorySize) {
        return Result<std::vector<ParsedEntry>>::failure("ZIP 中央目录大小不一致");
    }
    return Result<std::vector<ParsedEntry>>::success(std::move(entries));
}

[[nodiscard]] std::size_t pathDepth(const std::filesystem::path& path) {
    return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

[[nodiscard]] std::string entryKey(const ZipArchiveEntry& entry) {
    auto key = entry.relativePath.generic_string();
    while (!key.empty() && key.back() == '/') {
        key.pop_back();
    }
    return key;
}

[[nodiscard]] bool exceedsRatio(std::uint64_t expanded, std::uint64_t compressed,
                                double maximum) {
    if (expanded == 0U) {
        return false;
    }
    if (compressed == 0U) {
        return true;
    }
    return static_cast<long double>(expanded) /
               static_cast<long double>(compressed) >
           static_cast<long double>(maximum);
}

[[nodiscard]] Result<void> validateEntries(const std::vector<ParsedEntry>& entries,
                                           std::uint64_t archiveBytes,
                                           const ImportLimits& limits) {
    if (limits.maxFileCount == 0U || limits.maxSingleFileBytes == 0U ||
        limits.maxExpandedBytes == 0U || limits.maxTotalBytes == 0U ||
        limits.maxPathDepth == 0U || !std::isfinite(limits.maxCompressionRatio) ||
        limits.maxCompressionRatio < 1.0) {
        return Result<void>::failure("ZIP 导入资源预算配置无效");
    }

    std::set<std::string> paths;
    std::set<std::string> files;
    std::uint64_t expandedTotal = 0U;
    std::uint64_t compressedTotal = 0U;
    for (const auto& item : entries) {
        const auto& entry = item.entry;
        const auto pathText = entry.relativePath.generic_string();
        if (!PathGuard::isSafeArchiveEntry(pathText)) {
            return Result<void>::failure("ZIP 条目越过工作区边界: " + pathText);
        }
        const auto key = entryKey(entry);
        if (!paths.insert(key).second) {
            return Result<void>::failure("ZIP 包含重复目标条目: " + key);
        }
        if (entry.directory) {
            continue;
        }
        files.insert(key);
        if (entry.uncompressedSize >
            std::numeric_limits<std::uint64_t>::max() - expandedTotal) {
            return Result<void>::failure("ZIP 条目声明展开总量溢出");
        }
        expandedTotal += entry.uncompressedSize;
        if (entry.compressedSize > archiveBytes - std::min(archiveBytes, compressedTotal)) {
            return Result<void>::failure("ZIP 条目压缩大小异常: " + pathText);
        }
        compressedTotal += entry.compressedSize;
        if (exceedsRatio(entry.uncompressedSize, entry.compressedSize,
                         limits.maxCompressionRatio)) {
            return Result<void>::failure("ZIP 条目压缩比超过安全上限: " + pathText);
        }
    }
    if (exceedsRatio(expandedTotal, compressedTotal, limits.maxCompressionRatio)) {
        return Result<void>::failure("ZIP 总压缩比超过安全上限");
    }
    for (const auto& file : files) {
        auto parent = std::filesystem::path{file}.parent_path();
        while (!parent.empty()) {
            if (files.contains(parent.generic_string())) {
                return Result<void>::failure("ZIP 文件与目录目标冲突: " + file);
            }
            parent = parent.parent_path();
        }
    }
    return Result<void>::success();
}

[[nodiscard]] Result<std::vector<unsigned char>>
inflateRawDeflate(const unsigned char* compressed, std::size_t compressedSize,
                  std::size_t uncompressedSize) {
    if (compressedSize > static_cast<std::size_t>(std::numeric_limits<uInt>::max()) ||
        uncompressedSize > static_cast<std::size_t>(std::numeric_limits<uInt>::max())) {
        return Result<std::vector<unsigned char>>::failure("ZIP 条目超过 zlib 单次解压上限");
    }
    std::vector<unsigned char> output(uncompressedSize);
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed);
    stream.avail_in = static_cast<uInt>(compressedSize);
    unsigned char emptyOutput = 0U;
    stream.next_out = output.empty() ? &emptyOutput : output.data();
    stream.avail_out = output.empty() ? 1U : static_cast<uInt>(output.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return Result<std::vector<unsigned char>>::failure("初始化 ZIP deflate 解压器失败");
    }
    const int status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.total_out != output.size()) {
        return Result<std::vector<unsigned char>>::failure("ZIP deflate 数据解压失败");
    }
    return Result<std::vector<unsigned char>>::success(std::move(output));
}

[[nodiscard]] Result<std::vector<unsigned char>>
entryBytes(const std::vector<unsigned char>& data, const ParsedEntry& parsed) {
    const auto& entry = parsed.entry;
    const auto localOffset = static_cast<std::size_t>(parsed.localHeaderOffset);
    if (!rangeInside(localOffset, kLocalHeaderSize, data.size()) ||
        readU32(data, localOffset) != kLocalFileHeader) {
        return Result<std::vector<unsigned char>>::failure("ZIP 本地文件头损坏: " +
                                                           entry.relativePath.generic_string());
    }
    const auto localFlags = readU16(data, localOffset + 6U);
    const auto localMethod = readU16(data, localOffset + 8U);
    const auto fileNameLength = static_cast<std::size_t>(readU16(data, localOffset + 26U));
    const auto extraLength = static_cast<std::size_t>(readU16(data, localOffset + 28U));
    if (!rangeInside(localOffset + kLocalHeaderSize, fileNameLength + extraLength, data.size())) {
        return Result<std::vector<unsigned char>>::failure("ZIP 本地文件头越过边界");
    }
    const auto localNameOffset = localOffset + kLocalHeaderSize;
    const std::string localName{reinterpret_cast<const char*>(data.data() + localNameOffset),
                                fileNameLength};
    if (localName != entry.relativePath.generic_string() || localFlags != parsed.flags ||
        localMethod != entry.compressionMethod) {
        return Result<std::vector<unsigned char>>::failure("ZIP 本地文件头与中央目录不一致: " +
                                                           entry.relativePath.generic_string());
    }
    if ((localFlags & kDataDescriptorFlag) == 0U &&
        (readU32(data, localOffset + 14U) != parsed.crc ||
         readU32(data, localOffset + 18U) != entry.compressedSize ||
         readU32(data, localOffset + 22U) != entry.uncompressedSize)) {
        return Result<std::vector<unsigned char>>::failure("ZIP 条目大小或校验信息不一致: " +
                                                           entry.relativePath.generic_string());
    }
    const auto payloadOffset = localNameOffset + fileNameLength + extraLength;
    const auto compressedSize = static_cast<std::size_t>(entry.compressedSize);
    if (!rangeInside(payloadOffset, compressedSize, data.size())) {
        return Result<std::vector<unsigned char>>::failure("ZIP 条目数据越过文件边界: " +
                                                           entry.relativePath.generic_string());
    }

    Result<std::vector<unsigned char>> bytes =
        Result<std::vector<unsigned char>>::failure("不支持的 ZIP 压缩方法");
    if (entry.compressionMethod == kStoreMethod) {
        if (entry.compressedSize != entry.uncompressedSize) {
            return Result<std::vector<unsigned char>>::failure("ZIP store 条目大小不一致");
        }
        bytes = Result<std::vector<unsigned char>>::success(std::vector<unsigned char>{
            data.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
            data.begin() + static_cast<std::ptrdiff_t>(payloadOffset + compressedSize)});
    } else if (entry.compressionMethod == kDeflateMethod) {
        bytes = inflateRawDeflate(data.data() + payloadOffset, compressedSize,
                                  static_cast<std::size_t>(entry.uncompressedSize));
    }
    if (!bytes.ok()) {
        return bytes;
    }
    auto crc = crc32(0L, Z_NULL, 0);
    if (!bytes.value().empty()) {
        crc = crc32(crc, bytes.value().data(), static_cast<uInt>(bytes.value().size()));
    }
    if (static_cast<std::uint32_t>(crc) != parsed.crc) {
        return Result<std::vector<unsigned char>>::failure("ZIP 条目 CRC 校验失败: " +
                                                           entry.relativePath.generic_string());
    }
    return bytes;
}

[[nodiscard]] Result<void> prepareDestination(const std::filesystem::path& root) {
    std::error_code error;
    if (std::filesystem::exists(root, error)) {
        if (!std::filesystem::is_directory(root, error) || !std::filesystem::is_empty(root, error)) {
            return Result<void>::failure("ZIP 解压目标必须为空，拒绝覆盖既有文件");
        }
        return Result<void>::success();
    }
    std::filesystem::create_directories(root, error);
    if (error) {
        return Result<void>::failure("无法创建 ZIP 解压目录: " + error.message());
    }
    return Result<void>::success();
}

} // namespace

Result<std::vector<ZipArchiveEntry>>
ZipArchiveReader::list(const std::filesystem::path& archivePath,
                       const ImportLimits& limits) const {
    const auto data = readBinaryFile(archivePath, limits);
    if (!data.ok()) {
        return Result<std::vector<ZipArchiveEntry>>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<std::vector<ZipArchiveEntry>>::failure(parsed.error());
    }
    const auto valid = validateEntries(parsed.value(), data.value().size(), limits);
    if (!valid.ok()) {
        return Result<std::vector<ZipArchiveEntry>>::failure(valid.error());
    }
    std::vector<ZipArchiveEntry> entries;
    entries.reserve(parsed.value().size());
    for (const auto& item : parsed.value()) {
        entries.push_back(item.entry);
    }
    return Result<std::vector<ZipArchiveEntry>>::success(std::move(entries));
}

Result<ArchiveExtractionOutcome>
ZipArchiveReader::extractAll(const ZipExtractionRequest& request) const {
    const auto data = readBinaryFile(request.archivePath, request.limits);
    if (!data.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(parsed.error());
    }
    const auto valid = validateEntries(parsed.value(), data.value().size(), request.limits);
    if (!valid.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(valid.error());
    }

    ExtractionRollback rollback{request.destinationRoot};
    const auto prepared = prepareDestination(request.destinationRoot);
    if (!prepared.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(prepared.error());
    }
    rollback.arm();

    ArchiveExtractionOutcome outcome;
    std::uint64_t expandedTotal = 0U;
    std::error_code error;
    for (const auto& item : parsed.value()) {
        const auto target = request.destinationRoot / item.entry.relativePath;
        if (!PathGuard::isInsideRoot(request.destinationRoot, target)) {
            return Result<ArchiveExtractionOutcome>::failure(
                "ZIP 解压目标越过工作区边界: " + item.entry.relativePath.generic_string());
        }
        if (item.entry.directory) {
            if (pathDepth(item.entry.relativePath) > request.limits.maxPathDepth) {
                continue;
            }
            std::filesystem::create_directories(target, error);
            if (error) {
                return Result<ArchiveExtractionOutcome>::failure(
                    "无法创建 ZIP 目录: " + error.message());
            }
            continue;
        }
        if (outcome.files.size() >= request.limits.maxFileCount) {
            ++outcome.omittedFileCount;
            continue;
        }
        outcome.files.push_back(item.entry.relativePath);
        std::string deferredReason;
        if (pathDepth(item.entry.relativePath) > request.limits.maxPathDepth) {
            deferredReason = "PATH_DEPTH_LIMIT";
        } else if (item.entry.symlink) {
            deferredReason = "SYMLINK_DEFERRED";
        } else if (ArchiveExtractor::isArchivePath(item.entry.relativePath)) {
            deferredReason = "NESTED_ARCHIVE_DEFERRED";
        } else if ((item.flags & kEncryptedFlag) != 0U) {
            deferredReason = "ENCRYPTED_ENTRY_DEFERRED";
        } else if (item.entry.compressionMethod != kStoreMethod &&
                   item.entry.compressionMethod != kDeflateMethod) {
            deferredReason = "UNSUPPORTED_COMPRESSION_DEFERRED";
        } else if (item.entry.uncompressedSize > request.limits.maxSingleFileBytes) {
            deferredReason = "LARGE_BINARY_DEFERRED";
        } else {
            const auto expandedLimit =
                std::min(request.limits.maxExpandedBytes, request.limits.maxTotalBytes);
            if (item.entry.uncompressedSize > expandedLimit - expandedTotal) {
                deferredReason = "COPY_BUDGET_DEFERRED";
            }
        }
        if (!deferredReason.empty()) {
            outcome.deferredFiles.push_back({.relativePath = item.entry.relativePath,
                                             .sizeBytes = item.entry.uncompressedSize,
                                             .reason = std::move(deferredReason)});
            continue;
        }
        const auto bytes = entryBytes(data.value(), item);
        if (!bytes.ok()) {
            return Result<ArchiveExtractionOutcome>::failure(bytes.error());
        }
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            return Result<ArchiveExtractionOutcome>::failure(
                "无法创建 ZIP 子目录: " + error.message());
        }
        if (std::filesystem::exists(target, error)) {
            return Result<ArchiveExtractionOutcome>::failure(
                "ZIP 目标文件重复，拒绝覆盖: " + item.entry.relativePath.generic_string());
        }
        std::ofstream output(target, std::ios::binary);
        if (!output) {
            return Result<ArchiveExtractionOutcome>::failure(
                "无法写入 ZIP 条目: " + item.entry.relativePath.generic_string());
        }
        if (!bytes.value().empty()) {
            output.write(reinterpret_cast<const char*>(bytes.value().data()),
                         static_cast<std::streamsize>(bytes.value().size()));
        }
        output.flush();
        if (!output) {
            return Result<ArchiveExtractionOutcome>::failure(
                "ZIP 条目写入不完整: " + item.entry.relativePath.generic_string());
        }
        output.close();
        if (!output) {
            return Result<ArchiveExtractionOutcome>::failure(
                "ZIP 条目关闭失败: " + item.entry.relativePath.generic_string());
        }
        expandedTotal += item.entry.uncompressedSize;
    }
    std::sort(outcome.files.begin(), outcome.files.end());
    if (!outcome.deferredFiles.empty()) {
        outcome.warnings.push_back(
            "压缩包中有 " + std::to_string(outcome.deferredFiles.size()) +
            " 个大型、加密、嵌套或受限条目仅保留元数据，其他文件已正常解包");
    }
    if (outcome.omittedFileCount > 0U) {
        outcome.warnings.push_back("压缩包条目超过文件树预算，另有 " +
                                   std::to_string(outcome.omittedFileCount) + " 个未展开条目");
    }
    rollback.commit();
    return Result<ArchiveExtractionOutcome>::success(std::move(outcome));
}

Result<std::string> ZipArchiveReader::readTextEntry(const ZipEntryReadRequest& request) const {
    const auto data = readBinaryFile(request.archivePath, request.limits);
    if (!data.ok()) {
        return Result<std::string>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<std::string>::failure(parsed.error());
    }
    const auto valid = validateEntries(parsed.value(), data.value().size(), request.limits);
    if (!valid.ok()) {
        return Result<std::string>::failure(valid.error());
    }
    const auto expected = request.entryPath.generic_string();
    for (const auto& item : parsed.value()) {
        if (item.entry.relativePath.generic_string() != expected) {
            continue;
        }
        if (item.entry.directory || item.entry.symlink) {
            return Result<std::string>::failure("ZIP 条目不是普通文本文件: " + expected);
        }
        if ((item.flags & kEncryptedFlag) != 0U) {
            return Result<std::string>::failure("ZIP 文本条目已加密: " + expected);
        }
        if (item.entry.compressionMethod != kStoreMethod &&
            item.entry.compressionMethod != kDeflateMethod) {
            return Result<std::string>::failure("ZIP 文本条目使用了不支持的压缩方法: " +
                                               expected);
        }
        if (request.maxBytes == 0U || item.entry.uncompressedSize > request.maxBytes) {
            return Result<std::string>::failure("ZIP 文本条目超过读取上限: " + expected);
        }
        auto bytes = entryBytes(data.value(), item);
        if (!bytes.ok()) {
            return Result<std::string>::failure(bytes.error());
        }
        return Result<std::string>::success(std::string{
            reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()});
    }
    return Result<std::string>::failure("ZIP 条目不存在: " + expected);
}

} // namespace cc
