/**
 * @file ZipArchiveReader.cpp
 * @brief zip 压缩包目录读取与安全解压实现。
 */

#include "cc/loader/ZipArchiveReader.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

#include <zlib.h>

namespace cc {
namespace {

constexpr std::uint32_t kEndOfCentralDirectory = 0x06054b50U;
constexpr std::uint32_t kCentralDirectoryHeader = 0x02014b50U;
constexpr std::uint32_t kLocalFileHeader = 0x04034b50U;
constexpr std::uint16_t kStoreMethod = 0U;
constexpr std::uint16_t kDeflateMethod = 8U;
constexpr std::uint16_t kEncryptedFlag = 0x0001U;
constexpr std::size_t kEocdMinimumSize = 22U;
constexpr std::size_t kCentralHeaderSize = 46U;
constexpr std::size_t kLocalHeaderSize = 30U;
constexpr std::uint64_t kMaxEntrySize = 512ULL * 1024ULL * 1024ULL;

struct ParsedEntry {
    ZipArchiveEntry entry;
    std::uint32_t localHeaderOffset{0};
    std::uint16_t flags{0};
};

[[nodiscard]] Result<std::vector<unsigned char>> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<std::vector<unsigned char>>::failure("无法读取 zip 文件: " +
                                                           util::pathString(path));
    }
    return Result<std::vector<unsigned char>>::success(std::vector<unsigned char>{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}});
}

[[nodiscard]] std::uint16_t readU16(const std::vector<unsigned char>& data, std::size_t offset) {
    const auto low = static_cast<std::uint16_t>(data.at(offset));
    const auto high = static_cast<std::uint16_t>(data.at(offset + 1U));
    return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

[[nodiscard]] std::uint32_t readU32(const std::vector<unsigned char>& data, std::size_t offset) {
    return static_cast<std::uint32_t>(readU16(data, offset)) |
           (static_cast<std::uint32_t>(readU16(data, offset + 2U)) << 16U);
}

[[nodiscard]] Result<std::size_t>
findEndOfCentralDirectory(const std::vector<unsigned char>& data) {
    if (data.size() < kEocdMinimumSize) {
        return Result<std::size_t>::failure("zip 文件过小或缺少中央目录");
    }
    const auto searchStart =
        data.size() > std::numeric_limits<std::uint16_t>::max() + kEocdMinimumSize
            ? data.size() - std::numeric_limits<std::uint16_t>::max() - kEocdMinimumSize
            : 0U;
    for (std::size_t offset = data.size() - kEocdMinimumSize + 1U; offset-- > searchStart;) {
        if (readU32(data, offset) == kEndOfCentralDirectory) {
            return Result<std::size_t>::success(offset);
        }
        if (offset == 0U) {
            break;
        }
    }
    return Result<std::size_t>::failure("zip 文件缺少中央目录结束记录");
}

[[nodiscard]] Result<std::vector<ParsedEntry>>
parseCentralDirectory(const std::vector<unsigned char>& data) {
    const auto eocd = findEndOfCentralDirectory(data);
    if (!eocd.ok()) {
        return Result<std::vector<ParsedEntry>>::failure(eocd.error());
    }
    const auto entryCount = readU16(data, eocd.value() + 10U);
    const auto directorySize = readU32(data, eocd.value() + 12U);
    const auto directoryOffset = readU32(data, eocd.value() + 16U);
    if (directoryOffset + directorySize > data.size()) {
        return Result<std::vector<ParsedEntry>>::failure("zip 中央目录越过文件边界");
    }

    std::vector<ParsedEntry> entries;
    entries.reserve(entryCount);
    std::size_t offset = directoryOffset;
    for (std::uint16_t index = 0; index < entryCount; ++index) {
        if (offset + kCentralHeaderSize > data.size() ||
            readU32(data, offset) != kCentralDirectoryHeader) {
            return Result<std::vector<ParsedEntry>>::failure("zip 中央目录条目损坏");
        }
        const auto flags = readU16(data, offset + 8U);
        const auto method = readU16(data, offset + 10U);
        const auto compressedSize = readU32(data, offset + 20U);
        const auto uncompressedSize = readU32(data, offset + 24U);
        const auto fileNameLength = readU16(data, offset + 28U);
        const auto extraLength = readU16(data, offset + 30U);
        const auto commentLength = readU16(data, offset + 32U);
        const auto externalAttributes = readU32(data, offset + 38U);
        const auto localHeaderOffset = readU32(data, offset + 42U);
        const auto nameOffset = offset + kCentralHeaderSize;
        if (nameOffset + fileNameLength > data.size()) {
            return Result<std::vector<ParsedEntry>>::failure("zip 条目名称越过文件边界");
        }
        std::string name;
        name.reserve(fileNameLength);
        for (std::size_t charIndex = 0; charIndex < fileNameLength; ++charIndex) {
            name.push_back(static_cast<char>(data.at(nameOffset + charIndex)));
        }
        if (compressedSize == std::numeric_limits<std::uint32_t>::max() ||
            uncompressedSize == std::numeric_limits<std::uint32_t>::max()) {
            return Result<std::vector<ParsedEntry>>::failure("当前版本不支持 Zip64 条目: " + name);
        }
        ParsedEntry parsed;
        parsed.entry.relativePath = name;
        parsed.entry.directory = !name.empty() && name.back() == '/';
        parsed.entry.symlink = ((externalAttributes >> 16U) & 0170000U) == 0120000U;
        parsed.entry.compressionMethod = method;
        parsed.entry.compressedSize = compressedSize;
        parsed.entry.uncompressedSize = uncompressedSize;
        parsed.localHeaderOffset = localHeaderOffset;
        parsed.flags = flags;
        entries.push_back(std::move(parsed));
        offset = nameOffset + fileNameLength + extraLength + commentLength;
    }
    return Result<std::vector<ParsedEntry>>::success(std::move(entries));
}

[[nodiscard]] Result<std::vector<unsigned char>>
inflateRawDeflate(const std::vector<unsigned char>& compressed, std::size_t uncompressedSize) {
    if (compressed.size() > std::numeric_limits<uInt>::max() ||
        uncompressedSize > std::numeric_limits<uInt>::max()) {
        return Result<std::vector<unsigned char>>::failure("zip 条目超过 zlib 单次解压上限");
    }
    auto compressedCopy = compressed;
    std::vector<unsigned char> output(uncompressedSize);
    z_stream stream{};
    stream.next_in = compressedCopy.data();
    stream.avail_in = static_cast<uInt>(compressedCopy.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return Result<std::vector<unsigned char>>::failure("初始化 zip deflate 解压器失败");
    }
    const int status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.total_out != output.size()) {
        return Result<std::vector<unsigned char>>::failure("zip deflate 数据解压失败");
    }
    return Result<std::vector<unsigned char>>::success(std::move(output));
}

[[nodiscard]] Result<std::vector<unsigned char>> entryBytes(const std::vector<unsigned char>& data,
                                                            const ParsedEntry& parsed) {
    const auto& entry = parsed.entry;
    if ((parsed.flags & kEncryptedFlag) != 0U) {
        return Result<std::vector<unsigned char>>::failure("不支持加密 zip 条目: " +
                                                           entry.relativePath.string());
    }
    if (entry.uncompressedSize > kMaxEntrySize) {
        return Result<std::vector<unsigned char>>::failure("zip 条目过大，需要人工确认: " +
                                                           entry.relativePath.string());
    }
    const auto localOffset = static_cast<std::size_t>(parsed.localHeaderOffset);
    if (localOffset + kLocalHeaderSize > data.size() ||
        readU32(data, localOffset) != kLocalFileHeader) {
        return Result<std::vector<unsigned char>>::failure("zip 本地文件头损坏: " +
                                                           entry.relativePath.string());
    }
    const auto fileNameLength = readU16(data, localOffset + 26U);
    const auto extraLength = readU16(data, localOffset + 28U);
    const auto payloadOffset = localOffset + kLocalHeaderSize + fileNameLength + extraLength;
    if (payloadOffset + entry.compressedSize > data.size()) {
        return Result<std::vector<unsigned char>>::failure("zip 条目数据越过文件边界: " +
                                                           entry.relativePath.string());
    }
    const auto compressedSize = static_cast<std::size_t>(entry.compressedSize);
    const auto uncompressedSize = static_cast<std::size_t>(entry.uncompressedSize);
    std::vector<unsigned char> compressedBytes;
    compressedBytes.reserve(compressedSize);
    for (std::size_t byteIndex = 0; byteIndex < compressedSize; ++byteIndex) {
        compressedBytes.push_back(data.at(payloadOffset + byteIndex));
    }
    if (entry.compressionMethod == kStoreMethod) {
        if (compressedSize != uncompressedSize) {
            return Result<std::vector<unsigned char>>::failure("zip store 条目大小不一致: " +
                                                               entry.relativePath.string());
        }
        return Result<std::vector<unsigned char>>::success(std::move(compressedBytes));
    }
    if (entry.compressionMethod == kDeflateMethod) {
        return inflateRawDeflate(compressedBytes, uncompressedSize);
    }
    return Result<std::vector<unsigned char>>::failure("不支持的 zip 压缩方法: " +
                                                       std::to_string(entry.compressionMethod));
}

} // namespace

Result<std::vector<ZipArchiveEntry>>
ZipArchiveReader::list(const std::filesystem::path& archivePath) const {
    const auto data = readBinaryFile(archivePath);
    if (!data.ok()) {
        return Result<std::vector<ZipArchiveEntry>>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<std::vector<ZipArchiveEntry>>::failure(parsed.error());
    }
    std::vector<ZipArchiveEntry> entries;
    entries.reserve(parsed.value().size());
    for (const auto& item : parsed.value()) {
        entries.push_back(item.entry);
    }
    return Result<std::vector<ZipArchiveEntry>>::success(std::move(entries));
}

Result<std::vector<std::filesystem::path>>
ZipArchiveReader::extractAll(const ZipExtractionRequest& request) const {
    const auto data = readBinaryFile(request.archivePath);
    if (!data.ok()) {
        return Result<std::vector<std::filesystem::path>>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<std::vector<std::filesystem::path>>::failure(parsed.error());
    }
    std::error_code error;
    std::filesystem::create_directories(request.destinationRoot, error);
    if (error) {
        return Result<std::vector<std::filesystem::path>>::failure("无法创建解压目录: " +
                                                                   error.message());
    }

    std::vector<std::filesystem::path> files;
    for (const auto& item : parsed.value()) {
        const auto target = request.destinationRoot / item.entry.relativePath;
        // 写入前再次检查根目录边界。ArchiveExtractor 已做过一次校验，这里保留第二道防线，
        // 防止未来复用 ZipArchiveReader 时绕过输入安全策略。
        if (!PathGuard::isInsideRoot(request.destinationRoot, target)) {
            return Result<std::vector<std::filesystem::path>>::failure(
                "zip 解压目标越过工作区边界: " + item.entry.relativePath.string());
        }
        if (item.entry.symlink) {
            return Result<std::vector<std::filesystem::path>>::failure(
                "zip 中包含符号链接，已阻断: " + item.entry.relativePath.string());
        }
        if (item.entry.directory) {
            std::filesystem::create_directories(target, error);
            if (error) {
                return Result<std::vector<std::filesystem::path>>::failure("无法创建 zip 目录: " +
                                                                           error.message());
            }
            continue;
        }
        auto bytes = entryBytes(data.value(), item);
        if (!bytes.ok()) {
            return Result<std::vector<std::filesystem::path>>::failure(bytes.error());
        }
        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            return Result<std::vector<std::filesystem::path>>::failure("无法创建 zip 子目录: " +
                                                                       error.message());
        }
        std::ofstream output(target, std::ios::binary);
        if (!output) {
            return Result<std::vector<std::filesystem::path>>::failure(
                "无法写入 zip 条目: " + item.entry.relativePath.string());
        }
        for (const auto byte : bytes.value()) {
            output.put(static_cast<char>(byte));
        }
        files.push_back(item.entry.relativePath);
    }
    return Result<std::vector<std::filesystem::path>>::success(std::move(files));
}

Result<std::string> ZipArchiveReader::readTextEntry(const ZipEntryReadRequest& request) const {
    const auto data = readBinaryFile(request.archivePath);
    if (!data.ok()) {
        return Result<std::string>::failure(data.error());
    }
    const auto parsed = parseCentralDirectory(data.value());
    if (!parsed.ok()) {
        return Result<std::string>::failure(parsed.error());
    }
    const auto expected = request.entryPath.generic_string();
    for (const auto& item : parsed.value()) {
        if (item.entry.relativePath.generic_string() != expected) {
            continue;
        }
        if (item.entry.directory || item.entry.symlink) {
            return Result<std::string>::failure("zip 条目不是普通文本文件: " + expected);
        }
        if (request.maxBytes > 0U && item.entry.uncompressedSize > request.maxBytes) {
            return Result<std::string>::failure("zip 文本条目超过读取上限: " + expected);
        }
        auto bytes = entryBytes(data.value(), item);
        if (!bytes.ok()) {
            return Result<std::string>::failure(bytes.error());
        }
        std::string text;
        text.reserve(bytes.value().size());
        for (const auto byte : bytes.value()) {
            text.push_back(static_cast<char>(byte));
        }
        return Result<std::string>::success(std::move(text));
    }
    return Result<std::string>::failure("zip 条目不存在: " + expected);
}

} // namespace cc
