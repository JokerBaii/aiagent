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

namespace cc {
namespace {

[[nodiscard]] bool isZipPath(const std::filesystem::path& path) {
    return util::lowerAscii(path.extension().string()) == ".zip";
}

[[nodiscard]] Result<void> validateZipEntries(const std::filesystem::path& archivePath) {
    ZipArchiveReader reader;
    auto entries = reader.list(archivePath);
    if (!entries.ok()) {
        return Result<void>::failure(entries.error());
    }
    for (const auto& entry : entries.value()) {
        const auto entryName = util::pathString(entry.relativePath);
        if (!PathGuard::isSafeArchiveEntry(entry.relativePath)) {
            return Result<void>::failure("压缩包条目越过工作区边界: " + entryName);
        }
        if (entry.symlink) {
            return Result<void>::failure("压缩包包含符号链接，需要人工确认: " + entryName);
        }
        if (ArchiveExtractor::isArchivePath(entry.relativePath)) {
            return Result<void>::failure("发现嵌套压缩包，需要人工确认后再解包: " + entryName);
        }
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void> validateLibArchiveEntries(const std::filesystem::path& archivePath) {
    LibArchiveReader reader;
    auto entries = reader.list(archivePath);
    if (!entries.ok()) {
        return Result<void>::failure(entries.error());
    }
    for (const auto& entry : entries.value()) {
        const auto entryName = util::pathString(entry.relativePath);
        if (!PathGuard::isSafeArchiveEntry(entry.relativePath)) {
            return Result<void>::failure("压缩包条目越过工作区边界: " + entryName);
        }
        if (entry.symlink) {
            return Result<void>::failure("压缩包包含符号链接，需要人工确认: " + entryName);
        }
        if (ArchiveExtractor::isArchivePath(entry.relativePath)) {
            return Result<void>::failure("发现嵌套压缩包，需要人工确认后再解包: " + entryName);
        }
    }
    return Result<void>::success();
}

[[nodiscard]] Result<std::vector<std::filesystem::path>>
extractFiles(const std::filesystem::path& archivePath, const std::filesystem::path& inputRoot) {
    if (isZipPath(archivePath)) {
        auto validated = validateZipEntries(archivePath);
        if (!validated.ok()) {
            return Result<std::vector<std::filesystem::path>>::failure(validated.error());
        }
        // zip 继续走内部 reader，保持既有中心目录校验和 zip-slip 双重防线。
        return ZipArchiveReader{}.extractAll(
            {.archivePath = archivePath, .destinationRoot = inputRoot});
    }

    auto validated = validateLibArchiveEntries(archivePath);
    if (!validated.ok()) {
        return Result<std::vector<std::filesystem::path>>::failure(validated.error());
    }
    // tar/tgz/gz/7z 等格式由 libarchive 解析，但写入仍限制在 workspace/input。
    return LibArchiveReader{}.extractAll(
        {.archivePath = archivePath, .destinationRoot = inputRoot});
}

} // namespace

Result<ProjectContext> ArchiveExtractor::extract(const ArchiveImportRequest& request) const {
    const auto& archivePath = request.archivePath;
    const auto& workspaceRoot = request.workspaceRoot;
    if (!isSupportedArchivePath(archivePath)) {
        return Result<ProjectContext>::failure("当前版本不支持该压缩格式: " +
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
    // 解包前已逐条校验路径、符号链接和嵌套压缩包；reader 不调用 shell，
    // 只把普通文件写入隔离工作区 input，避免压缩包导入绕过 ExecuteCommand 权限边界。
    auto extracted = extractFiles(archivePath, inputRoot);
    if (!extracted.ok()) {
        return Result<ProjectContext>::failure(extracted.error());
    }

    ProjectContext context;
    context.originalRoot = normalized.value();
    context.inputRoot = inputRoot;
    context.workspaceRoot = workspaceRoot;
    context.sessionId = workspaceRoot.filename().string();
    context.projectName = archivePath.stem().string();
    context.unpackStatus = isZipPath(archivePath) ? "ZIP_EXTRACTED" : "ARCHIVE_EXTRACTED";
    context.archiveInput = true;
    context.inputFiles = std::move(extracted.value());
    return Result<ProjectContext>::success(context);
}

bool ArchiveExtractor::isArchivePath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    const auto filename = util::lowerAscii(path.filename().string());
    return extension == ".zip" || extension == ".tar" || extension == ".gz" ||
           extension == ".tgz" || extension == ".7z" || extension == ".rar" ||
           filename.ends_with(".tar.gz");
}

bool ArchiveExtractor::isSupportedArchivePath(const std::filesystem::path& path) {
    const auto extension = util::lowerAscii(path.extension().string());
    const auto filename = util::lowerAscii(path.filename().string());
    return extension == ".zip" || extension == ".tar" || extension == ".gz" ||
           extension == ".tgz" || extension == ".7z" || filename.ends_with(".tar.gz");
}

} // namespace cc
