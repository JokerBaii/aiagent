#include "cc/loader/LibArchiveReader.hpp"

#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace cc {
namespace {

constexpr std::size_t kReadBufferSize = 64U * 1024U;

struct ArchiveReadDeleter {
    void operator()(archive* handle) const {
        if (handle != nullptr) {
            archive_read_free(handle);
        }
    }
};

using ArchiveReadHandle = std::unique_ptr<archive, ArchiveReadDeleter>;

enum class EntryWriteStatus {
    Complete,
    SingleFileLimit,
    TotalLimit,
    Failed,
};

struct EntryWriteOutcome {
    EntryWriteStatus status{EntryWriteStatus::Failed};
    std::uint64_t bytesSeen{0U};
    std::string error;
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

    void arm() {
        armed_ = true;
    }
    void commit() {
        committed_ = true;
    }

  private:
    std::filesystem::path root_;
    bool rootExisted_{false};
    bool armed_{false};
    bool committed_{false};
};

[[nodiscard]] std::string archiveError(archive* handle, const std::string& fallback) {
    const char* message = archive_error_string(handle);
    return message == nullptr ? fallback : std::string{message};
}

[[nodiscard]] Result<std::uint64_t> archiveSize(const std::filesystem::path& archivePath,
                                                const ImportLimits& limits) {
    std::error_code error;
    const auto size = std::filesystem::file_size(archivePath, error);
    if (error) {
        return Result<std::uint64_t>::failure("无法读取压缩包大小: " + error.message());
    }
    if (size > limits.maxArchiveBytes ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::uint64_t>::max())) {
        return Result<std::uint64_t>::failure("压缩包超过导入读取上限");
    }
    return Result<std::uint64_t>::success(static_cast<std::uint64_t>(size));
}

[[nodiscard]] Result<ArchiveReadHandle> openArchive(const std::filesystem::path& archivePath,
                                                    const ImportLimits& limits) {
    const auto size = archiveSize(archivePath, limits);
    if (!size.ok()) {
        return Result<ArchiveReadHandle>::failure(size.error());
    }

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
    const auto fileType = archive_entry_filetype(entry);
    item.directory = fileType == AE_IFDIR;
    item.symlink = fileType == AE_IFLNK || archive_entry_symlink(entry) != nullptr ||
                   archive_entry_hardlink(entry) != nullptr;
    const auto declaredSize = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : -1;
    item.sizeBytes = declaredSize < 0 ? 0U : static_cast<std::uint64_t>(declaredSize);
    return item;
}

[[nodiscard]] std::size_t pathDepth(const std::filesystem::path& path) {
    return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

[[nodiscard]] std::string entryKey(const std::filesystem::path& path) {
    auto key = path.lexically_normal().generic_string();
    while (!key.empty() && key.back() == '/') {
        key.pop_back();
    }
    return key;
}

class EntryPathIndex {
  public:
    [[nodiscard]] Result<void> add(const std::filesystem::path& path, bool directory) {
        const auto key = entryKey(path);
        if (!paths_.insert(key).second) {
            return Result<void>::failure("压缩包包含重复目标条目: " + key);
        }

        auto parent = path.lexically_normal().parent_path();
        while (!parent.empty()) {
            const auto parentKey = entryKey(parent);
            if (filePaths_.contains(parentKey)) {
                return Result<void>::failure("压缩包的文件与子路径冲突: " + parentKey);
            }
            parent = parent.parent_path();
        }

        if (!directory) {
            const auto descendantPrefix = key + '/';
            const auto descendant = paths_.lower_bound(descendantPrefix);
            if (descendant != paths_.end() && descendant->starts_with(descendantPrefix)) {
                return Result<void>::failure("压缩包的文件与子路径冲突: " + key);
            }
            filePaths_.insert(key);
        }
        return Result<void>::success();
    }

  private:
    std::set<std::string> paths_;
    std::set<std::string> filePaths_;
};

[[nodiscard]] Result<void> validateEntry(const LibArchiveEntry& entry, archive_entry* rawEntry) {
    const auto entryName = util::pathString(entry.relativePath);
    if (entry.relativePath.empty()) {
        return Result<void>::failure("压缩包条目名称为空");
    }
    if (!PathGuard::isSafeArchiveEntry(entry.relativePath)) {
        return Result<void>::failure("压缩包条目越过工作区边界: " + entryName);
    }
    if (archive_entry_size_is_set(rawEntry) && archive_entry_size(rawEntry) < 0) {
        return Result<void>::failure("压缩包条目声明了无效大小: " + entryName);
    }
    return Result<void>::success();
}

[[nodiscard]] bool exceedsRatio(std::uint64_t expanded, std::uint64_t compressed, double maximum) {
    if (expanded == 0U) {
        return false;
    }
    if (compressed == 0U) {
        return true;
    }
    return static_cast<long double>(expanded) / static_cast<long double>(compressed) >
           static_cast<long double>(maximum);
}

[[nodiscard]] Result<void> validateLimits(const ImportLimits& limits) {
    if (limits.maxFileCount == 0U || limits.maxSingleFileBytes == 0U ||
        limits.maxTotalBytes == 0U || limits.maxArchiveBytes == 0U ||
        limits.maxExpandedBytes == 0U || limits.maxPathDepth == 0U ||
        !std::isfinite(limits.maxCompressionRatio) || limits.maxCompressionRatio < 1.0) {
        return Result<void>::failure("压缩包导入资源预算配置无效");
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void> prepareDestination(const std::filesystem::path& root) {
    std::error_code error;
    const auto exists = std::filesystem::exists(root, error);
    if (error) {
        return Result<void>::failure("无法检查压缩包解压目录: " + error.message());
    }
    if (exists) {
        const auto isDirectory = std::filesystem::is_directory(root, error);
        if (error) {
            return Result<void>::failure("无法检查压缩包解压目录类型: " + error.message());
        }
        const auto isEmpty = isDirectory && std::filesystem::is_empty(root, error);
        if (error) {
            return Result<void>::failure("无法检查压缩包解压目录内容: " + error.message());
        }
        if (!isDirectory || !isEmpty) {
            return Result<void>::failure("压缩包解压目标必须为空，拒绝覆盖既有文件");
        }
        return Result<void>::success();
    }
    std::filesystem::create_directories(root, error);
    if (error) {
        return Result<void>::failure("无法创建压缩包解压目录: " + error.message());
    }
    return Result<void>::success();
}

[[nodiscard]] EntryWriteOutcome writeEntryData(
    archive* handle, const std::filesystem::path& target, const std::filesystem::path& displayPath,
    const ImportLimits& limits, std::uint64_t archiveBytes, std::uint64_t expandedBefore,
    std::uint64_t securityExpandedBefore, bool declaredSizeKnown, std::uint64_t declaredSize) {
    std::ofstream output(target, std::ios::binary);
    if (!output) {
        return {.status = EntryWriteStatus::Failed,
                .bytesSeen = 0U,
                .error = "无法写入压缩包条目: " + util::pathString(displayPath)};
    }

    const auto expandedLimit = std::min(limits.maxExpandedBytes, limits.maxTotalBytes);
    std::uint64_t entryBytes = 0U;
    std::vector<char> buffer(kReadBufferSize);
    while (true) {
        const auto bytesRead = archive_read_data(handle, buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return {.status = EntryWriteStatus::Failed,
                    .bytesSeen = entryBytes,
                    .error = archiveError(handle, "压缩包条目读取失败")};
        }
        if (bytesRead == 0) {
            break;
        }
        const auto chunk = static_cast<std::uint64_t>(bytesRead);
        if (chunk > std::numeric_limits<std::uint64_t>::max() - entryBytes) {
            return {.status = EntryWriteStatus::Failed,
                    .bytesSeen = entryBytes,
                    .error = "压缩包条目实际展开大小溢出: " + util::pathString(displayPath)};
        }
        const auto nextEntryBytes = entryBytes + chunk;
        if (declaredSizeKnown && nextEntryBytes > declaredSize) {
            return {.status = EntryWriteStatus::Failed,
                    .bytesSeen = nextEntryBytes,
                    .error = "压缩包条目实际大小超过声明大小: " + util::pathString(displayPath)};
        }
        if (!declaredSizeKnown) {
            if (nextEntryBytes >
                    std::numeric_limits<std::uint64_t>::max() - securityExpandedBefore ||
                exceedsRatio(securityExpandedBefore + nextEntryBytes, archiveBytes,
                             limits.maxCompressionRatio)) {
                return {.status = EntryWriteStatus::Failed,
                        .bytesSeen = nextEntryBytes,
                        .error = "压缩包实际展开比超过安全上限"};
            }
        }
        if (nextEntryBytes > limits.maxSingleFileBytes) {
            return {.status = EntryWriteStatus::SingleFileLimit,
                    .bytesSeen = nextEntryBytes,
                    .error = {}};
        }
        if (expandedBefore > expandedLimit ||
            nextEntryBytes > expandedLimit - std::min(expandedBefore, expandedLimit)) {
            return {
                .status = EntryWriteStatus::TotalLimit, .bytesSeen = nextEntryBytes, .error = {}};
        }
        output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!output) {
            return {.status = EntryWriteStatus::Failed,
                    .bytesSeen = entryBytes,
                    .error = "写入压缩包条目失败: " + util::pathString(displayPath)};
        }
        entryBytes = nextEntryBytes;
    }
    if (declaredSizeKnown && entryBytes != declaredSize) {
        return {.status = EntryWriteStatus::Failed,
                .bytesSeen = entryBytes,
                .error = "压缩包条目实际大小与声明大小不一致: " + util::pathString(displayPath)};
    }
    output.flush();
    if (!output) {
        return {.status = EntryWriteStatus::Failed,
                .bytesSeen = entryBytes,
                .error = "压缩包条目写入不完整: " + util::pathString(displayPath)};
    }
    output.close();
    if (!output) {
        return {.status = EntryWriteStatus::Failed,
                .bytesSeen = entryBytes,
                .error = "压缩包条目关闭失败: " + util::pathString(displayPath)};
    }
    return {.status = EntryWriteStatus::Complete, .bytesSeen = entryBytes, .error = {}};
}

[[nodiscard]] Result<void> skipEntryData(archive* handle, const std::string& fallback) {
    if (archive_read_data_skip(handle) != ARCHIVE_OK) {
        return Result<void>::failure(archiveError(handle, fallback));
    }
    return Result<void>::success();
}

[[nodiscard]] Result<std::uint64_t> consumeUnknownSizeEntry(archive* handle,
                                                            std::uint64_t archiveBytes,
                                                            std::uint64_t securityExpandedBefore,
                                                            double maximumRatio,
                                                            std::uint64_t bytesAlreadySeen = 0U) {
    auto entryBytes = bytesAlreadySeen;
    std::vector<char> buffer(kReadBufferSize);
    while (true) {
        const auto bytesRead = archive_read_data(handle, buffer.data(), buffer.size());
        if (bytesRead < 0) {
            return Result<std::uint64_t>::failure(archiveError(handle, "压缩包条目安全校验失败"));
        }
        if (bytesRead == 0) {
            break;
        }
        const auto chunk = static_cast<std::uint64_t>(bytesRead);
        if (chunk > std::numeric_limits<std::uint64_t>::max() - entryBytes) {
            return Result<std::uint64_t>::failure("压缩包条目实际展开大小溢出");
        }
        entryBytes += chunk;
        if (entryBytes > std::numeric_limits<std::uint64_t>::max() - securityExpandedBefore ||
            exceedsRatio(securityExpandedBefore + entryBytes, archiveBytes, maximumRatio)) {
            return Result<std::uint64_t>::failure("压缩包实际展开比超过安全上限");
        }
    }
    return Result<std::uint64_t>::success(entryBytes);
}

} // namespace

Result<std::vector<LibArchiveEntry>>
LibArchiveReader::list(const std::filesystem::path& archivePath, const ImportLimits& limits) const {
    const auto validLimits = validateLimits(limits);
    if (!validLimits.ok()) {
        return Result<std::vector<LibArchiveEntry>>::failure(validLimits.error());
    }
    const auto size = archiveSize(archivePath, limits);
    if (!size.ok()) {
        return Result<std::vector<LibArchiveEntry>>::failure(size.error());
    }
    auto opened = openArchive(archivePath, limits);
    if (!opened.ok()) {
        return Result<std::vector<LibArchiveEntry>>::failure(opened.error());
    }

    std::vector<LibArchiveEntry> entries;
    EntryPathIndex pathIndex;
    std::uint64_t declaredTotal = 0U;
    archive_entry* rawEntry = nullptr;
    while (true) {
        const auto status = archive_read_next_header(opened.value().get(), &rawEntry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            return Result<std::vector<LibArchiveEntry>>::failure(
                archiveError(opened.value().get(), "读取压缩包条目失败"));
        }
        const auto entry = toEntry(rawEntry);
        const auto valid = validateEntry(entry, rawEntry);
        if (!valid.ok()) {
            return Result<std::vector<LibArchiveEntry>>::failure(valid.error());
        }
        const auto registered = pathIndex.add(entry.relativePath, entry.directory);
        if (!registered.ok()) {
            return Result<std::vector<LibArchiveEntry>>::failure(registered.error());
        }
        if (!entry.directory && archive_entry_size_is_set(rawEntry)) {
            if (entry.sizeBytes > std::numeric_limits<std::uint64_t>::max() - declaredTotal) {
                return Result<std::vector<LibArchiveEntry>>::failure("压缩包声明展开总量溢出");
            }
            declaredTotal += entry.sizeBytes;
            if (exceedsRatio(declaredTotal, size.value(), limits.maxCompressionRatio)) {
                return Result<std::vector<LibArchiveEntry>>::failure(
                    "压缩包声明展开比超过安全上限");
            }
        }
        if (entries.size() < limits.maxFileCount) {
            entries.push_back(entry);
        }
        const auto skipped = skipEntryData(opened.value().get(), "跳过压缩包条目失败");
        if (!skipped.ok()) {
            return Result<std::vector<LibArchiveEntry>>::failure(skipped.error());
        }
    }
    return Result<std::vector<LibArchiveEntry>>::success(std::move(entries));
}

Result<ArchiveExtractionOutcome>
LibArchiveReader::extractAll(const LibArchiveExtractionRequest& request) const {
    const auto validLimits = validateLimits(request.limits);
    if (!validLimits.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(validLimits.error());
    }
    const auto size = archiveSize(request.archivePath, request.limits);
    if (!size.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(size.error());
    }
    auto opened = openArchive(request.archivePath, request.limits);
    if (!opened.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(opened.error());
    }

    ExtractionRollback rollback{request.destinationRoot};
    const auto prepared = prepareDestination(request.destinationRoot);
    if (!prepared.ok()) {
        return Result<ArchiveExtractionOutcome>::failure(prepared.error());
    }
    rollback.arm();

    ArchiveExtractionOutcome outcome;
    EntryPathIndex pathIndex;
    std::uint64_t expandedTotal = 0U;
    std::uint64_t securityExpandedTotal = 0U;
    std::size_t materializedDirectoryCount = 0U;
    std::size_t deferredDirectoryCount = 0U;
    archive_entry* rawEntry = nullptr;
    while (true) {
        const auto status = archive_read_next_header(opened.value().get(), &rawEntry);
        if (status == ARCHIVE_EOF) {
            break;
        }
        if (status != ARCHIVE_OK) {
            return Result<ArchiveExtractionOutcome>::failure(
                archiveError(opened.value().get(), "读取压缩包条目失败"));
        }

        const auto entry = toEntry(rawEntry);
        const auto valid = validateEntry(entry, rawEntry);
        if (!valid.ok()) {
            return Result<ArchiveExtractionOutcome>::failure(valid.error());
        }
        const auto registered = pathIndex.add(entry.relativePath, entry.directory);
        if (!registered.ok()) {
            return Result<ArchiveExtractionOutcome>::failure(registered.error());
        }

        const auto target = request.destinationRoot / entry.relativePath;
        if (!PathGuard::isInsideRoot(request.destinationRoot, target)) {
            return Result<ArchiveExtractionOutcome>::failure("压缩包解包目标越过工作区边界: " +
                                                             util::pathString(entry.relativePath));
        }
        const auto declaredSizeKnown = archive_entry_size_is_set(rawEntry) != 0;
        const auto fileType = archive_entry_filetype(rawEntry);
        const auto encryptionStatus = archive_entry_is_encrypted(rawEntry);
        if (!entry.directory && declaredSizeKnown) {
            if (entry.sizeBytes >
                std::numeric_limits<std::uint64_t>::max() - securityExpandedTotal) {
                return Result<ArchiveExtractionOutcome>::failure("压缩包声明展开总量溢出");
            }
            securityExpandedTotal += entry.sizeBytes;
            if (exceedsRatio(securityExpandedTotal, size.value(),
                             request.limits.maxCompressionRatio)) {
                return Result<ArchiveExtractionOutcome>::failure("压缩包声明展开比超过安全上限");
            }
        }

        std::error_code error;
        if (entry.directory) {
            if (pathDepth(entry.relativePath) > request.limits.maxPathDepth) {
                ++deferredDirectoryCount;
                const auto skipped = skipEntryData(opened.value().get(), "跳过过深压缩包目录失败");
                if (!skipped.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(skipped.error());
                }
                continue;
            }
            if (materializedDirectoryCount >= request.limits.maxFileCount) {
                ++outcome.omittedFileCount;
                const auto skipped =
                    skipEntryData(opened.value().get(), "跳过预算外压缩包目录失败");
                if (!skipped.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(skipped.error());
                }
                continue;
            }
            std::filesystem::create_directories(target, error);
            if (error) {
                return Result<ArchiveExtractionOutcome>::failure("无法创建压缩包目录: " +
                                                                 error.message());
            }
            ++materializedDirectoryCount;
            const auto skipped = skipEntryData(opened.value().get(), "跳过压缩包目录失败");
            if (!skipped.ok()) {
                return Result<ArchiveExtractionOutcome>::failure(skipped.error());
            }
            continue;
        }
        if (outcome.files.size() >= request.limits.maxFileCount) {
            ++outcome.omittedFileCount;
            if (!declaredSizeKnown && fileType == AE_IFREG && encryptionStatus == 0) {
                const auto consumed = consumeUnknownSizeEntry(opened.value().get(), size.value(),
                                                              securityExpandedTotal,
                                                              request.limits.maxCompressionRatio);
                if (!consumed.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(consumed.error());
                }
                securityExpandedTotal += consumed.value();
            } else {
                const auto skipped =
                    skipEntryData(opened.value().get(), "跳过预算外压缩包条目失败");
                if (!skipped.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(skipped.error());
                }
            }
            continue;
        }
        outcome.files.push_back(entry.relativePath);
        const auto expandedLimit =
            std::min(request.limits.maxExpandedBytes, request.limits.maxTotalBytes);

        std::string deferredReason;
        if (pathDepth(entry.relativePath) > request.limits.maxPathDepth) {
            deferredReason = "PATH_DEPTH_LIMIT";
        } else if (entry.symlink) {
            deferredReason = "SYMLINK_DEFERRED";
        } else if (fileType != AE_IFREG) {
            deferredReason = "NON_REGULAR_ENTRY_DEFERRED";
        } else if (ArchiveExtractor::isArchivePath(entry.relativePath)) {
            deferredReason = "NESTED_ARCHIVE_DEFERRED";
        } else if (encryptionStatus == 1) {
            deferredReason = "ENCRYPTED_ENTRY_DEFERRED";
        } else if (encryptionStatus < 0) {
            deferredReason = "ENCRYPTION_STATUS_UNKNOWN_DEFERRED";
        } else if (entry.sizeBytes > request.limits.maxSingleFileBytes) {
            deferredReason = "LARGE_BINARY_DEFERRED";
        } else if (entry.sizeBytes > expandedLimit - std::min(expandedTotal, expandedLimit)) {
            deferredReason = "COPY_BUDGET_DEFERRED";
        }
        if (!deferredReason.empty()) {
            auto deferredSize = entry.sizeBytes;
            if (!declaredSizeKnown && fileType == AE_IFREG && encryptionStatus == 0) {
                const auto consumed = consumeUnknownSizeEntry(opened.value().get(), size.value(),
                                                              securityExpandedTotal,
                                                              request.limits.maxCompressionRatio);
                if (!consumed.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(consumed.error());
                }
                securityExpandedTotal += consumed.value();
                deferredSize = consumed.value();
            } else {
                const auto skipped = skipEntryData(opened.value().get(), "跳过受限压缩包条目失败");
                if (!skipped.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(skipped.error());
                }
            }
            outcome.deferredFiles.push_back({.relativePath = entry.relativePath,
                                             .sizeBytes = deferredSize,
                                             .reason = std::move(deferredReason)});
            continue;
        }

        std::filesystem::create_directories(target.parent_path(), error);
        if (error) {
            return Result<ArchiveExtractionOutcome>::failure("无法创建压缩包子目录: " +
                                                             error.message());
        }
        const auto targetExists = std::filesystem::exists(target, error);
        if (error) {
            return Result<ArchiveExtractionOutcome>::failure("无法检查压缩包目标文件: " +
                                                             error.message());
        }
        if (targetExists) {
            return Result<ArchiveExtractionOutcome>::failure("压缩包目标文件重复，拒绝覆盖: " +
                                                             util::pathString(entry.relativePath));
        }
        const auto written = writeEntryData(
            opened.value().get(), target, entry.relativePath, request.limits, size.value(),
            expandedTotal, securityExpandedTotal, declaredSizeKnown, entry.sizeBytes);
        if (written.status == EntryWriteStatus::Failed) {
            return Result<ArchiveExtractionOutcome>::failure(written.error);
        }
        if (written.status == EntryWriteStatus::SingleFileLimit ||
            written.status == EntryWriteStatus::TotalLimit) {
            error.clear();
            std::filesystem::remove(target, error);
            if (error) {
                return Result<ArchiveExtractionOutcome>::failure("无法移除超限条目的临时文件: " +
                                                                 error.message());
            }
            auto deferredSize = std::max(entry.sizeBytes, written.bytesSeen);
            if (!declaredSizeKnown) {
                const auto consumed = consumeUnknownSizeEntry(
                    opened.value().get(), size.value(), securityExpandedTotal,
                    request.limits.maxCompressionRatio, written.bytesSeen);
                if (!consumed.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(consumed.error());
                }
                securityExpandedTotal += consumed.value();
                deferredSize = consumed.value();
            } else {
                const auto skipped = skipEntryData(opened.value().get(), "跳过超限压缩包条目失败");
                if (!skipped.ok()) {
                    return Result<ArchiveExtractionOutcome>::failure(skipped.error());
                }
            }
            outcome.deferredFiles.push_back(
                {.relativePath = entry.relativePath,
                 .sizeBytes = deferredSize,
                 .reason = written.status == EntryWriteStatus::SingleFileLimit
                               ? "RUNTIME_SINGLE_FILE_LIMIT_DEFERRED"
                               : "RUNTIME_COPY_BUDGET_DEFERRED"});
            continue;
        }
        if (written.bytesSeen > std::numeric_limits<std::uint64_t>::max() - expandedTotal) {
            return Result<ArchiveExtractionOutcome>::failure("压缩包实际写入总量溢出");
        }
        expandedTotal += written.bytesSeen;
        if (!declaredSizeKnown) {
            if (written.bytesSeen >
                std::numeric_limits<std::uint64_t>::max() - securityExpandedTotal) {
                return Result<ArchiveExtractionOutcome>::failure("压缩包实际展开总量溢出");
            }
            securityExpandedTotal += written.bytesSeen;
        }
    }
    std::sort(outcome.files.begin(), outcome.files.end());
    if (!outcome.deferredFiles.empty()) {
        outcome.warnings.push_back("压缩包中有 " + std::to_string(outcome.deferredFiles.size()) +
                                   " 个大型、嵌套或受限条目仅保留元数据，其他文件已正常解包");
    }
    if (outcome.omittedFileCount > 0U) {
        outcome.warnings.push_back("压缩包条目超过文件树预算，另有 " +
                                   std::to_string(outcome.omittedFileCount) + " 个未展开条目");
    }
    if (deferredDirectoryCount > 0U) {
        outcome.warnings.push_back("压缩包中有 " + std::to_string(deferredDirectoryCount) +
                                   " 个过深空目录未创建");
    }
    rollback.commit();
    return Result<ArchiveExtractionOutcome>::success(std::move(outcome));
}

} // namespace cc
