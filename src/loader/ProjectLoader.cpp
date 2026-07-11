/**
 * @file ProjectLoader.cpp
 * @brief 项目目录加载与工作区创建实现。
 */

#include "cc/loader/ProjectLoader.hpp"
#include "cc/inventory/GeneratedVendoredDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/TimeUtil.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>

namespace cc {
namespace {

struct CopyOutcome {
    std::vector<std::filesystem::path> files;
    std::vector<DeferredInputFile> deferredFiles;
    std::vector<std::string> warnings;
};

class WorkspaceRollback {
  public:
    explicit WorkspaceRollback(std::filesystem::path workspace)
        : workspace_{std::move(workspace)} {}

    ~WorkspaceRollback() {
        if (!committed_) {
            std::error_code ignored;
            std::filesystem::remove_all(workspace_, ignored);
        }
    }

    void commit() {
        committed_ = true;
    }

  private:
    std::filesystem::path workspace_;
    bool committed_{false};
};

[[nodiscard]] Result<void> validateLimits(const ImportLimits& limits) {
    if (limits.maxFileCount == 0U || limits.maxSingleFileBytes == 0U ||
        limits.maxTotalBytes == 0U || limits.maxArchiveBytes == 0U ||
        limits.maxExpandedBytes == 0U || limits.maxPathDepth == 0U ||
        !std::isfinite(limits.maxCompressionRatio) || limits.maxCompressionRatio < 1.0) {
        return Result<void>::failure("项目导入资源预算配置无效");
    }
    return Result<void>::success();
}

[[nodiscard]] std::size_t pathDepth(const std::filesystem::path& path) {
    return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

[[nodiscard]] Result<std::filesystem::path>
relativeEntryPath(const std::filesystem::path& path, const std::filesystem::path& sourceRoot) {
    const auto relative = path.lexically_normal().lexically_relative(sourceRoot.lexically_normal());
    if (relative.empty() || relative.is_absolute()) {
        return Result<std::filesystem::path>::failure("项目条目无法映射到安全相对路径");
    }
    for (const auto& component : relative) {
        if (component == "." || component == "..") {
            return Result<std::filesystem::path>::failure("项目条目路径越过导入根目录");
        }
    }
    return Result<std::filesystem::path>::success(relative);
}

[[nodiscard]] std::string deferredReason(std::filesystem::file_type type) {
    switch (type) {
    case std::filesystem::file_type::symlink:
        return "SYMLINK_DEFERRED";
    case std::filesystem::file_type::fifo:
        return "FIFO_DEFERRED";
    case std::filesystem::file_type::socket:
        return "SOCKET_DEFERRED";
    case std::filesystem::file_type::block:
        return "BLOCK_DEVICE_DEFERRED";
    case std::filesystem::file_type::character:
        return "CHARACTER_DEVICE_DEFERRED";
    default:
        return "NON_REGULAR_FILE_DEFERRED";
    }
}

[[nodiscard]] bool shouldSkipDirectory(const std::filesystem::path& relativePath) {
    const auto name = relativePath.filename().string();
    if (name == ".git" || name == ".workspaces") {
        return true;
    }
    const GeneratedVendoredDetector detector;
    return detector.isGenerated(relativePath) || detector.isVendored(relativePath);
}

[[nodiscard]] std::filesystem::path workspaceBase() {
    const auto* configured = std::getenv("CONTEST_WORKSPACE_ROOT");
    if (configured != nullptr && *configured != '\0') {
        return std::filesystem::path{configured};
    }
    return std::filesystem::current_path() / ".workspaces";
}

[[nodiscard]] Result<std::uint64_t> copyFileBounded(const std::filesystem::path& source,
                                                    const std::filesystem::path& target,
                                                    std::uint64_t expectedBytes,
                                                    std::uint64_t singleFileLimit,
                                                    std::uint64_t remainingTotal) {
    std::error_code statusError;
    const auto status = std::filesystem::symlink_status(source, statusError);
    if (statusError || status.type() != std::filesystem::file_type::regular) {
        return Result<std::uint64_t>::failure("待复制材料不再是普通文件");
    }
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return Result<std::uint64_t>::failure("无法打开待复制材料");
    }
    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) {
        return Result<std::uint64_t>::failure("无法打开工作区目标");
    }
    std::array<char, 64U * 1024U> buffer{};
    std::uint64_t copied = 0U;
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count <= 0) {
            break;
        }
        const auto chunk = static_cast<std::uint64_t>(count);
        if (chunk > singleFileLimit - std::min(copied, singleFileLimit) ||
            chunk > remainingTotal - std::min(copied, remainingTotal)) {
            return Result<std::uint64_t>::failure("材料在复制过程中超过导入资源上限");
        }
        output.write(buffer.data(), count);
        if (!output) {
            return Result<std::uint64_t>::failure("材料写入工作区失败");
        }
        copied += chunk;
    }
    if (!input.eof()) {
        return Result<std::uint64_t>::failure("读取材料时发生错误");
    }
    output.flush();
    output.close();
    if (!output) {
        return Result<std::uint64_t>::failure("材料未完整写入工作区");
    }
    if (copied != expectedBytes) {
        return Result<std::uint64_t>::failure("材料在导入过程中发生变化，请重试");
    }
    return Result<std::uint64_t>::success(copied);
}

[[nodiscard]] Result<CopyOutcome> copyDirectoryInput(const std::filesystem::path& sourceRoot,
                                                     const std::filesystem::path& inputRoot,
                                                     const ImportLimits& limits) {
    CopyOutcome outcome;
    std::uint64_t totalBytes = 0U;
    std::size_t skippedDirectories = 0U;
    std::size_t deferredLinks = 0U;
    std::size_t deferredSpecialFiles = 0U;
    std::size_t deferredDeepFiles = 0U;
    std::size_t deferredLargeFiles = 0U;
    std::size_t deferredBudgetFiles = 0U;
    std::size_t deferredOtherFiles = 0U;
    std::size_t unreadableDirectories = 0U;
    std::size_t manifestEntryCount = 0U;
    bool manifestLimitReached = false;
    std::filesystem::path firstOmittedPath;
    std::error_code ec;
    std::filesystem::create_directories(inputRoot, ec);
    if (ec) {
        return Result<CopyOutcome>::failure("无法创建工作区输入目录: " + ec.message());
    }

    // 手动维护待扫描目录，使一个不可读子目录不会终止其余兄弟目录的导入。所有条目先用
    // symlink_status 分类；只有确认是普通文件后才会读取大小或内容，因此不会跟随链接，
    // 也不会因 FIFO、socket 或设备文件阻塞。
    std::vector<std::filesystem::path> pendingDirectories{sourceRoot};
    while (!pendingDirectories.empty() && !manifestLimitReached) {
        auto directory = std::move(pendingDirectories.back());
        pendingDirectories.pop_back();

        const auto currentStatus = std::filesystem::symlink_status(directory, ec);
        if (ec || currentStatus.type() != std::filesystem::file_type::directory) {
            if (directory == sourceRoot) {
                return Result<CopyOutcome>::failure("项目根目录在导入前已不可读或类型已变化");
            }
            ++unreadableDirectories;
            ec.clear();
            continue;
        }

        std::filesystem::directory_iterator iter(directory,
                                                 std::filesystem::directory_options::none, ec);
        if (ec) {
            if (directory == sourceRoot) {
                return Result<CopyOutcome>::failure("项目根目录不可读: " + ec.message());
            }
            ++unreadableDirectories;
            ec.clear();
            continue;
        }

        // directory_iterator 的原始顺序由文件系统决定。保留当前预算内字典序最小的条目，
        // 再排序处理，保证容量预算和清单预算每次都选择同一批文件，同时限制临时内存。
        const auto entryLess = [](const std::filesystem::directory_entry& left,
                                  const std::filesystem::directory_entry& right) {
            return left.path().filename().generic_string() <
                   right.path().filename().generic_string();
        };
        const auto remainingEntries = limits.maxFileCount - manifestEntryCount;
        const auto selectionLimit = remainingEntries < std::numeric_limits<std::size_t>::max()
                                        ? remainingEntries + 1U
                                        : remainingEntries;
        std::vector<std::filesystem::directory_entry> entries;
        entries.reserve(std::min<std::size_t>(selectionLimit, 4'096U));
        bool directoryHadMoreEntries = false;
        const std::filesystem::directory_iterator end;
        while (iter != end) {
            if (entries.size() < selectionLimit) {
                entries.push_back(*iter);
                std::push_heap(entries.begin(), entries.end(), entryLess);
            } else {
                directoryHadMoreEntries = true;
                if (selectionLimit > 0U && entryLess(*iter, entries.front())) {
                    std::pop_heap(entries.begin(), entries.end(), entryLess);
                    entries.back() = *iter;
                    std::push_heap(entries.begin(), entries.end(), entryLess);
                }
            }
            iter.increment(ec);
            if (ec) {
                ++unreadableDirectories;
                ec.clear();
                break;
            }
        }
        std::sort(entries.begin(), entries.end(), entryLess);

        std::vector<std::filesystem::path> childDirectories;
        for (const auto& entry : entries) {
            const auto path = entry.path();
            auto relativeResult = relativeEntryPath(path, sourceRoot);
            if (!relativeResult.ok()) {
                return Result<CopyOutcome>::failure(relativeResult.error() + ": " +
                                                    util::pathString(path));
            }
            const auto relative = relativeResult.value();

            if (manifestEntryCount >= limits.maxFileCount) {
                manifestLimitReached = true;
                firstOmittedPath = relative;
                break;
            }
            ++manifestEntryCount;

            std::error_code statusError;
            const auto status = entry.symlink_status(statusError);
            if (!statusError && status.type() == std::filesystem::file_type::directory) {
                if (shouldSkipDirectory(relative)) {
                    ++skippedDirectories;
                } else {
                    childDirectories.push_back(path);
                }
            } else {
                outcome.files.push_back(relative);

                if (statusError) {
                    ++deferredOtherFiles;
                    outcome.deferredFiles.push_back(
                        {.relativePath = relative, .reason = "FILE_METADATA_UNREADABLE"});
                } else if (status.type() == std::filesystem::file_type::symlink) {
                    ++deferredLinks;
                    outcome.deferredFiles.push_back(
                        {.relativePath = relative, .reason = "SYMLINK_DEFERRED"});
                } else if (status.type() != std::filesystem::file_type::regular) {
                    ++deferredSpecialFiles;
                    outcome.deferredFiles.push_back(
                        {.relativePath = relative, .reason = deferredReason(status.type())});
                } else {
                    const auto size = std::filesystem::file_size(path, ec);
                    if (ec) {
                        ++deferredOtherFiles;
                        outcome.deferredFiles.push_back(
                            {.relativePath = relative, .reason = "FILE_METADATA_UNREADABLE"});
                        ec.clear();
                    } else if (pathDepth(relative) > limits.maxPathDepth) {
                        ++deferredDeepFiles;
                        outcome.deferredFiles.push_back({.relativePath = relative,
                                                         .sizeBytes = size,
                                                         .reason = "PATH_DEPTH_LIMIT"});
                    } else if (size > limits.maxSingleFileBytes) {
                        ++deferredLargeFiles;
                        outcome.deferredFiles.push_back({.relativePath = relative,
                                                         .sizeBytes = size,
                                                         .reason = "LARGE_BINARY_DEFERRED"});
                    } else {
                        const auto size64 = static_cast<std::uint64_t>(size);
                        if (size64 > limits.maxTotalBytes - totalBytes) {
                            ++deferredBudgetFiles;
                            outcome.deferredFiles.push_back({.relativePath = relative,
                                                             .sizeBytes = size,
                                                             .reason = "COPY_BUDGET_DEFERRED"});
                        } else {
                            const auto target = inputRoot / relative;
                            if (!PathGuard::isInsideRoot(inputRoot, target)) {
                                return Result<CopyOutcome>::failure("复制目标越过工作区边界: " +
                                                                    util::pathString(relative));
                            }

                            std::filesystem::create_directories(target.parent_path(), ec);
                            if (ec) {
                                std::error_code typeError;
                                const auto parentStatus = std::filesystem::symlink_status(
                                    target.parent_path(), typeError);
                                if (!typeError &&
                                    parentStatus.type() != std::filesystem::file_type::not_found &&
                                    parentStatus.type() != std::filesystem::file_type::directory) {
                                    return Result<CopyOutcome>::failure(
                                        "工作区目录与文件路径冲突: " + util::pathString(relative));
                                }
                                ++deferredOtherFiles;
                                outcome.deferredFiles.push_back({.relativePath = relative,
                                                                 .sizeBytes = size,
                                                                 .reason = "COPY_FAILED"});
                                ec.clear();
                            } else {
                                const bool targetExists = std::filesystem::exists(target, ec);
                                if (ec) {
                                    ++deferredOtherFiles;
                                    outcome.deferredFiles.push_back({.relativePath = relative,
                                                                     .sizeBytes = size,
                                                                     .reason = "COPY_FAILED"});
                                    ec.clear();
                                } else if (targetExists) {
                                    return Result<CopyOutcome>::failure("工作区目标文件重复: " +
                                                                        util::pathString(relative));
                                } else {
                                    const auto copied = copyFileBounded(
                                        path, target, size64, limits.maxSingleFileBytes,
                                        limits.maxTotalBytes - totalBytes);
                                    if (!copied.ok()) {
                                        std::filesystem::remove(target, ec);
                                        ec.clear();
                                        ++deferredOtherFiles;
                                        outcome.deferredFiles.push_back({.relativePath = relative,
                                                                         .sizeBytes = size,
                                                                         .reason = "COPY_FAILED"});
                                    } else {
                                        totalBytes += copied.value();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!manifestLimitReached && directoryHadMoreEntries) {
            manifestLimitReached = true;
            const auto relativeDirectory = directory.lexically_relative(sourceRoot);
            firstOmittedPath = relativeDirectory.empty() || relativeDirectory == "."
                                   ? std::filesystem::path{"…"}
                                   : relativeDirectory / "…";
        }
        for (auto child = childDirectories.rbegin(); child != childDirectories.rend(); ++child) {
            pendingDirectories.push_back(*child);
        }
    }

    if (manifestLimitReached) {
        outcome.warnings.push_back("项目文件清单达到 " + std::to_string(limits.maxFileCount) +
                                   " 个条目的有界预算；从 “" + util::pathString(firstOmittedPath) +
                                   "” 起的其余条目未扫描或复制，可提高导入预算后重新导入");
    }
    if (skippedDirectories > 0U) {
        outcome.warnings.push_back("已跳过 " + std::to_string(skippedDirectories) +
                                   " 个生成物或第三方依赖目录。");
    }
    if (deferredLinks > 0U) {
        outcome.warnings.push_back("已识别 " + std::to_string(deferredLinks) +
                                   " 个符号链接；未跟随目标，仅保留链接路径元数据");
    }
    if (deferredSpecialFiles > 0U) {
        outcome.warnings.push_back("已识别 " + std::to_string(deferredSpecialFiles) +
                                   " 个管道、socket 或设备等非普通文件；未打开内容");
    }
    if (deferredDeepFiles > 0U) {
        outcome.warnings.push_back("已识别 " + std::to_string(deferredDeepFiles) +
                                   " 个路径层级过深的文件；仅保留元数据");
    }
    if (deferredLargeFiles > 0U) {
        outcome.warnings.push_back("已识别 " + std::to_string(deferredLargeFiles) +
                                   " 个超大文件；保留格式、大小和路径信息，暂不复制或展开内容");
    }
    if (deferredBudgetFiles > 0U) {
        outcome.warnings.push_back("工作副本达到容量预算，另有 " +
                                   std::to_string(deferredBudgetFiles) +
                                   " 个文件仅保留元数据，可在智能体按需读取时再处理");
    }
    if (deferredOtherFiles > 0U) {
        outcome.warnings.push_back("另有 " + std::to_string(deferredOtherFiles) +
                                   " 个文件无法安全复制，仅保留元数据并等待人工确认");
    }
    if (unreadableDirectories > 0U) {
        outcome.warnings.push_back("有 " + std::to_string(unreadableDirectories) +
                                   " 个子目录无法继续读取；其他可访问目录已正常导入");
    }
    std::sort(outcome.files.begin(), outcome.files.end());
    std::sort(outcome.deferredFiles.begin(), outcome.deferredFiles.end(),
              [](const DeferredInputFile& left, const DeferredInputFile& right) {
                  return left.relativePath.generic_string() < right.relativePath.generic_string();
              });
    return Result<CopyOutcome>::success(std::move(outcome));
}

[[nodiscard]] Result<ProjectContext>
makeMetadataOnlyContext(const std::filesystem::path& source, const std::filesystem::path& workspace,
                        std::string sessionId, std::uintmax_t size, std::string status,
                        std::string reason, std::string warning, bool archiveInput) {
    std::error_code error;
    const auto inputRoot = workspace / "input";
    std::filesystem::create_directories(inputRoot, error);
    if (error) {
        return Result<ProjectContext>::failure("无法创建工作区输入目录: " + error.message());
    }
    std::filesystem::create_directories(workspace / "repaired", error);
    if (error) {
        return Result<ProjectContext>::failure("无法创建修复工作区: " + error.message());
    }

    ProjectContext context;
    context.originalRoot = source;
    context.inputRoot = inputRoot;
    context.workspaceRoot = workspace;
    context.sessionId = std::move(sessionId);
    context.projectName = source.stem().string();
    context.unpackStatus = std::move(status);
    context.archiveInput = archiveInput;
    context.inputFiles.push_back(source.filename());
    context.deferredFiles.push_back(
        {.relativePath = source.filename(), .sizeBytes = size, .reason = std::move(reason)});
    context.warnings.push_back(std::move(warning));
    return Result<ProjectContext>::success(std::move(context));
}

} // namespace

ProjectLoader::ProjectLoader(ImportLimits limits) : limits_{limits} {}

Result<ProjectContext> ProjectLoader::load(const std::filesystem::path& projectPath) const {
    const auto validLimits = validateLimits(limits_);
    if (!validLimits.ok()) {
        return Result<ProjectContext>::failure(validLimits.error());
    }
    const auto sessionId = util::makeSessionId();
    const auto workspace = workspaceBase() / sessionId;
    std::error_code ec;
    if (std::filesystem::exists(workspace, ec)) {
        return Result<ProjectContext>::failure("审计工作区会话标识冲突，请重试");
    }
    if (ec) {
        return Result<ProjectContext>::failure("无法检查审计工作区: " + ec.message());
    }
    WorkspaceRollback rollback{workspace};

    auto absoluteInput = std::filesystem::absolute(projectPath, ec);
    if (ec) {
        return Result<ProjectContext>::failure("无法定位项目路径: " + ec.message());
    }
    absoluteInput = absoluteInput.lexically_normal();
    const auto inputStatus = std::filesystem::symlink_status(absoluteInput, ec);
    if (inputStatus.type() == std::filesystem::file_type::not_found) {
        return Result<ProjectContext>::failure("项目路径不存在: " + util::pathString(projectPath));
    }
    if (ec) {
        return Result<ProjectContext>::failure("无法读取项目路径类型: " + ec.message());
    }
    if (inputStatus.type() == std::filesystem::file_type::symlink ||
        (inputStatus.type() != std::filesystem::file_type::regular &&
         inputStatus.type() != std::filesystem::file_type::directory)) {
        const bool isLink = inputStatus.type() == std::filesystem::file_type::symlink;
        auto metadata = makeMetadataOnlyContext(
            absoluteInput, workspace, sessionId, 0U, "INPUT_METADATA_ONLY",
            isLink ? "SYMLINK_DEFERRED" : deferredReason(inputStatus.type()),
            isLink ? "所选路径是符号链接；为避免越过用户选择边界，未跟随目标，仅保留元数据"
                   : "所选路径不是普通文件或目录；为避免阻塞或设备访问，未打开内容",
            false);
        if (!metadata.ok()) {
            return metadata;
        }
        rollback.commit();
        return metadata;
    }

    const auto normalized = PathGuard::normalize(absoluteInput);
    if (!normalized.ok()) {
        return Result<ProjectContext>::failure(normalized.error());
    }

    if (ArchiveExtractor::isArchivePath(normalized.value()) &&
        std::filesystem::is_regular_file(normalized.value(), ec)) {
        const auto size = std::filesystem::file_size(normalized.value(), ec);
        if (ec) {
            auto metadata = makeMetadataOnlyContext(
                normalized.value(), workspace, sessionId, 0U, "ARCHIVE_METADATA_ONLY",
                "FILE_METADATA_UNREADABLE",
                "无法安全读取归档大小，已保留路径和格式信息，未尝试展开", true);
            if (!metadata.ok()) {
                return metadata;
            }
            rollback.commit();
            return metadata;
        }
        if (!ArchiveExtractor::isSupportedArchivePath(normalized.value()) ||
            size > limits_.maxArchiveBytes) {
            const bool unsupported = !ArchiveExtractor::isSupportedArchivePath(normalized.value());
            auto metadata = makeMetadataOnlyContext(
                normalized.value(), workspace, sessionId, size, "ARCHIVE_METADATA_ONLY",
                unsupported ? "UNSUPPORTED_ARCHIVE_FORMAT" : "ARCHIVE_TOO_LARGE_FOR_INDEXING",
                unsupported ? "已识别该归档文件，但当前解析器无法安全展开；文件仍保留在清单中"
                            : "归档文件超过自动展开预算，已保留格式、大小和路径信息",
                true);
            if (!metadata.ok()) {
                return metadata;
            }
            rollback.commit();
            return metadata;
        }

        auto extracted = ArchiveExtractor{}.extract(
            {.archivePath = normalized.value(), .workspaceRoot = workspace, .limits = limits_});
        if (!extracted.ok()) {
            return extracted;
        }
        std::filesystem::create_directories(workspace / "repaired", ec);
        if (ec) {
            return Result<ProjectContext>::failure("无法创建修复工作区: " + ec.message());
        }
        rollback.commit();
        return extracted;
    }

    const auto inputRoot = workspace / "input";
    if (std::filesystem::is_regular_file(normalized.value(), ec)) {
        const auto size = std::filesystem::file_size(normalized.value(), ec);
        if (ec) {
            auto metadata = makeMetadataOnlyContext(
                normalized.value(), workspace, sessionId, 0U, "SINGLE_FILE_METADATA_ONLY",
                "FILE_METADATA_UNREADABLE", "无法安全读取文件大小，已保留路径和格式信息", false);
            if (!metadata.ok()) {
                return metadata;
            }
            rollback.commit();
            return metadata;
        }
        const bool metadataOnly = size > limits_.maxSingleFileBytes || size > limits_.maxTotalBytes;
        if (metadataOnly) {
            const bool exceedsSingleFileLimit = size > limits_.maxSingleFileBytes;
            auto metadata = makeMetadataOnlyContext(
                normalized.value(), workspace, sessionId, size, "SINGLE_FILE_METADATA_ONLY",
                exceedsSingleFileLimit ? "LARGE_BINARY_DEFERRED" : "COPY_BUDGET_DEFERRED",
                exceedsSingleFileLimit
                    ? "该文件超过单文件读取预算，已识别格式、大小和路径，暂不复制内容"
                    : "该文件超过本次工作副本容量预算，已保留格式、大小和路径信息",
                false);
            if (!metadata.ok()) {
                return metadata;
            }
            rollback.commit();
            return metadata;
        }
        std::filesystem::create_directories(inputRoot, ec);
        if (ec) {
            return Result<ProjectContext>::failure("无法创建工作区输入目录: " + ec.message());
        }
        const auto target = inputRoot / normalized.value().filename();
        const auto copied =
            copyFileBounded(normalized.value(), target, static_cast<std::uint64_t>(size),
                            limits_.maxSingleFileBytes, limits_.maxTotalBytes);
        if (!copied.ok()) {
            std::filesystem::remove(target, ec);
            ec.clear();
            auto metadata = makeMetadataOnlyContext(
                normalized.value(), workspace, sessionId, size, "SINGLE_FILE_METADATA_ONLY",
                "COPY_FAILED", "文件暂时无法安全复制；已保留元数据，可修复权限后重试", false);
            if (!metadata.ok()) {
                return metadata;
            }
            rollback.commit();
            return metadata;
        }
        std::filesystem::create_directories(workspace / "repaired", ec);
        if (ec) {
            return Result<ProjectContext>::failure("无法创建修复工作区: " + ec.message());
        }

        ProjectContext context;
        context.originalRoot = normalized.value();
        context.inputRoot = inputRoot;
        context.workspaceRoot = workspace;
        context.sessionId = sessionId;
        context.projectName = normalized.value().stem().string();
        context.unpackStatus = "SINGLE_FILE_COPIED_TO_WORKSPACE";
        context.inputFiles.push_back(normalized.value().filename());
        rollback.commit();
        return Result<ProjectContext>::success(std::move(context));
    }
    if (!std::filesystem::is_directory(normalized.value(), ec)) {
        return Result<ProjectContext>::failure("项目路径不是可审计的文件或目录: " +
                                               util::pathString(projectPath));
    }

    auto copied = copyDirectoryInput(normalized.value(), inputRoot, limits_);
    if (!copied.ok()) {
        return Result<ProjectContext>::failure(copied.error());
    }
    std::filesystem::create_directories(workspace / "repaired", ec);
    if (ec) {
        return Result<ProjectContext>::failure("无法创建修复工作区: " + ec.message());
    }

    ProjectContext context;
    context.originalRoot = normalized.value();
    context.inputRoot = inputRoot;
    context.workspaceRoot = workspace;
    context.sessionId = sessionId;
    context.projectName = context.originalRoot.filename().string();
    context.unpackStatus = "DIRECTORY_COPIED_TO_WORKSPACE";
    context.inputFiles = std::move(copied.value().files);
    context.deferredFiles = std::move(copied.value().deferredFiles);
    context.warnings = std::move(copied.value().warnings);
    rollback.commit();
    return Result<ProjectContext>::success(std::move(context));
}

} // namespace cc
