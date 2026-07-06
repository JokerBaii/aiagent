/**
 * @file LibArchiveReader.cpp
 * @brief libarchive 压缩包安全读取实现。
 */

#include "cc/loader/LibArchiveReader.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace cc {
namespace {

constexpr std::uint64_t kMaxArchiveEntrySize = 512ULL * 1024ULL * 1024ULL;
constexpr std::size_t kReadBufferSize = static_cast<std::size_t>(64U) * 1024U;

struct ArchiveReadDeleter {
    void operator()(archive* handle) const {
        if (handle != nullptr) {
            archive_read_free(handle);
        }
    }
};

using ArchiveReadHandle = std::unique_ptr<archive, ArchiveReadDeleter>;

[[nodiscard]] std::string archiveError(archive* handle, const std::string& fallback) {
    const char* message = archive_error_string(handle);
    return message == nullptr ? fallback : std::string{message};
}

[[nodiscard]] Result<ArchiveReadHandle> openArchive(const std::filesystem::path& archivePath) {
    ArchiveReadHandle handle{archive_read_new()};
    if (handle == nullptr) {
        return Result<ArchiveReadHandle>::failure("无法创建 libarchive reader");
    }
    archive_read_support_filter_all(handle.get());
    archive_read_support_format_all(handle.get());
    archive_read_support_format_raw(handle.get());

    const auto path = util::pathString(archivePath);
    if (archive_read_open_filename(handle.get(), path.c_str(), kReadBufferSize) != ARCHIVE_OK) {
        return Result<ArchiveReadHandle>::failure(
            archiveError(handle.get(), "无法打开压缩包: " + path));
    }
    return Result<ArchiveReadHandle>::success(std::move(handle));
}

[[nodiscard]] std::filesystem::path entryPath(archive_entry* entry) {
    const char* utf8Path = archive_entry_pathname_utf8(entry);
    if (utf8Path != nullptr) {
        return std::filesystem::path{utf8Path};
    }
    const char* rawPath = archive_entry_pathname(entry);
    return rawPath == nullptr ? std::filesystem::path{} : std::filesystem::path{rawPath};
}

[[nodiscard]] LibArchiveEntry toEntry(archive_entry* entry) {
    LibArchiveEntry item;
    item.relativePath = entryPath(entry);
    item.directory = archive_entry_filetype(entry) == AE_IFDIR;
    item.symlink = archive_entry_filetype(entry) == AE_IFLNK;
    item.sizeBytes = archive_entry_size_is_set(entry)
                         ? static_cast<std::uint64_t>(archive_entry_size(entry))
                         : 0U;
    return item;
}

[[nodiscard]] Result<void> validateEntry(const LibArchiveEntry& entry) {
    const auto entryName = util::pathString(entry.relativePath);
    if (entry.relativePath.empty()) {
        return Result<void>::failure("压缩包条目名称为空");
    }
    if (!PathGuard::isSafeArchiveEntry(entry.relativePath)) {
        return Result<void>::failure("压缩包条目越过工作区边界: " + entryName);
    }
    if (entry.symlink) {
        return Result<void>::failure("压缩包包含符号链接，需要人工确认: " + entryName);
    }
    if (ArchiveExtractor::isArchivePath(entry.relativePath)) {
        return Result<void>::failure("发现嵌套压缩包，需要人工确认后再解包: " + entryName);
    }
    if (entry.sizeBytes > kMaxArchiveEntrySize) {
        return Result<void>::failure("压缩包条目过大，需要人工确认: " + entryName);
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void> writeEntryData(archive* handle, const std::filesystem::path& target) {
    std::ofstream output(target, std::ios::binary);
    if (!output) {
        return Result<void>::failure("无法写入压缩包条目: " + util::pathString(target));
    }

    std::vector<char> buffer(kReadBufferSize);
    while (true) {
        const auto bytesRead = archive_read_data(handle, buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return Result<void>::failure(archiveError(handle, "压缩包条目读取失败"));
        }
        if (bytesRead == 0) {
            break;
        }
        output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!output) {
            return Result<void>::failure("写入压缩包条目失败: " + util::pathString(target));
        }
    }
    return Result<void>::success();
}

} // namespace

Result<std::vector<LibArchiveEntry>>
LibArchiveReader::list(const std::filesystem::path& archivePath) const {
    auto opened = openArchive(archivePath);
    if (!opened.ok()) {
        return Result<std::vector<LibArchiveEntry>>::failure(opened.error());
    }

    std::vector<LibArchiveEntry> entries;
    archive_entry* entry = nullptr;
    while (true) {
        const auto status = archive_read_next_header(opened.value().get(), &entry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            return Result<std::vector<LibArchiveEntry>>::failure(
                archiveError(opened.value().get(), "读取压缩包条目失败"));
        }
        entries.push_back(toEntry(entry));
        archive_read_data_skip(opened.value().get());
    }
    return Result<std::vector<LibArchiveEntry>>::success(std::move(entries));
}

Result<std::vector<std::filesystem::path>>
LibArchiveReader::extractAll(const LibArchiveExtractionRequest& request) const {
    auto opened = openArchive(request.archivePath);
    if (!opened.ok()) {
        return Result<std::vector<std::filesystem::path>>::failure(opened.error());
    }

    std::error_code error;
    std::filesystem::create_directories(request.destinationRoot, error);
    if (error) {
        return Result<std::vector<std::filesystem::path>>::failure("无法创建解包目录: " +
                                                                   error.message());
    }

    std::vector<std::filesystem::path> files;
    archive_entry* rawEntry = nullptr;
    while (true) {
        const auto status = archive_read_next_header(opened.value().get(), &rawEntry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            return Result<std::vector<std::filesystem::path>>::failure(
                archiveError(opened.value().get(), "读取压缩包条目失败"));
        }

        const auto entry = toEntry(rawEntry);
        auto validated = validateEntry(entry);
        if (!validated.ok()) {
            return Result<std::vector<std::filesystem::path>>::failure(validated.error());
        }

        const auto target = request.destinationRoot / entry.relativePath;
        // libarchive 支持的格式更多，写入前仍必须复核目标边界，防止 tar 等格式路径穿越。
        if (!PathGuard::isInsideRoot(request.destinationRoot, target)) {
            return Result<std::vector<std::filesystem::path>>::failure(
                "压缩包解包目标越过工作区边界: " + util::pathString(entry.relativePath));
        }
        if (entry.directory) {
            std::filesystem::create_directories(target, error);
            if (error) {
                return Result<std::vector<std::filesystem::path>>::failure("无法创建压缩包目录: " +
                                                                           error.message());
            }
            archive_read_data_skip(opened.value().get());
            continue;
        }
        if (archive_entry_filetype(rawEntry) != AE_IFREG) {
            return Result<std::vector<std::filesystem::path>>::failure(
                "压缩包包含非普通文件，需要人工确认: " + util::pathString(entry.relativePath));
        }

        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            return Result<std::vector<std::filesystem::path>>::failure("无法创建压缩包子目录: " +
                                                                       error.message());
        }
        auto written = writeEntryData(opened.value().get(), target);
        if (!written.ok()) {
            return Result<std::vector<std::filesystem::path>>::failure(written.error());
        }
        files.push_back(entry.relativePath);
    }
    return Result<std::vector<std::filesystem::path>>::success(std::move(files));
}

} // namespace cc
