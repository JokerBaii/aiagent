/**
 * @file ProjectLoader.cpp
 * @brief 项目目录加载与工作区创建实现。
 */

#include "cc/loader/ProjectLoader.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/TimeUtil.hpp"

namespace cc {
namespace {

struct CopyOutcome {
    std::vector<std::filesystem::path> files;
    std::vector<std::string> warnings;
};

[[nodiscard]] bool shouldSkipDirectory(const std::filesystem::path& path) {
    const auto name = path.filename().string();
    return name == ".git" || name == ".workspaces";
}

[[nodiscard]] Result<CopyOutcome> copyDirectoryInput(const std::filesystem::path& sourceRoot,
                                                     const std::filesystem::path& inputRoot) {
    CopyOutcome outcome;
    std::error_code ec;
    std::filesystem::create_directories(inputRoot, ec);
    if (ec) {
        return Result<CopyOutcome>::failure("无法创建工作区输入目录: " + ec.message());
    }

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iter(sourceRoot, options, ec);
         !ec && iter != std::filesystem::recursive_directory_iterator(); iter.increment(ec)) {
        const auto path = iter->path();
        if (iter->is_symlink(ec)) {
            outcome.warnings.push_back("跳过符号链接，防止工作区外文件被间接纳入: " +
                                       util::pathString(path));
            iter.disable_recursion_pending();
            continue;
        }
        if (iter->is_directory(ec)) {
            if (shouldSkipDirectory(path)) {
                iter.disable_recursion_pending();
            }
            continue;
        }
        if (!iter->is_regular_file(ec)) {
            continue;
        }
        const auto relative = std::filesystem::relative(path, sourceRoot, ec);
        if (ec) {
            return Result<CopyOutcome>::failure("计算项目相对路径失败: " + ec.message());
        }
        const auto target = inputRoot / relative;
        // 目录导入也先复制进隔离工作区，后续审计只读 workspace/input，避免修复流程误碰原项目。
        if (!PathGuard::isInsideRoot(inputRoot, target)) {
            return Result<CopyOutcome>::failure("复制目标越过工作区边界: " +
                                                util::pathString(relative));
        }
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            return Result<CopyOutcome>::failure("创建工作区子目录失败: " + ec.message());
        }
        std::filesystem::copy_file(path, target, std::filesystem::copy_options::overwrite_existing,
                                   ec);
        if (ec) {
            return Result<CopyOutcome>::failure("复制项目文件到工作区失败: " + ec.message());
        }
        outcome.files.push_back(relative);
    }
    if (ec) {
        outcome.warnings.push_back("复制项目时部分目录不可读: " + ec.message());
    }
    return Result<CopyOutcome>::success(std::move(outcome));
}

} // namespace

Result<ProjectContext> ProjectLoader::load(const std::filesystem::path& projectPath) const {
    const auto sessionId = util::makeSessionId();
    const auto workspace = std::filesystem::current_path() / ".workspaces" / sessionId;
    if (ArchiveExtractor::isArchivePath(projectPath)) {
        return ArchiveExtractor{}.extract({.archivePath = projectPath, .workspaceRoot = workspace});
    }

    std::error_code ec;
    if (!std::filesystem::exists(projectPath, ec) ||
        !std::filesystem::is_directory(projectPath, ec)) {
        return Result<ProjectContext>::failure("项目路径不存在或不是目录: " +
                                               util::pathString(projectPath));
    }

    const auto normalized = PathGuard::normalize(projectPath);
    if (!normalized.ok()) {
        return Result<ProjectContext>::failure(normalized.error());
    }

    const auto inputRoot = workspace / "input";
    auto copied = copyDirectoryInput(normalized.value(), inputRoot);
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
    context.warnings = std::move(copied.value().warnings);
    return Result<ProjectContext>::success(context);
}

} // namespace cc
