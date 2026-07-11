#include "cc/agent/WorkspaceEditor.hpp"

#include "cc/audit/AuditEngine.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/inventory/SensitiveFileDetector.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"
#include "cc/util/TimeUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <set>
#include <sstream>
#include <string_view>

namespace cc {
namespace {

constexpr std::size_t kMaximumEditableBytes = 1024U * 1024U;
constexpr std::size_t kMaximumChangedFiles = 1000U;
constexpr std::size_t kMaximumPatchBytes = 16U * 1024U * 1024U;

[[nodiscard]] std::filesystem::path repairedRoot(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / "repaired-project";
}

[[nodiscard]] std::filesystem::path manifestPath(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / "changed-paths.json";
}

[[nodiscard]] std::filesystem::path patchPath(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / "changes.patch";
}

[[nodiscard]] bool skippedDirectory(const std::filesystem::path& path) {
    const auto name = util::lowerAscii(path.filename().generic_string());
    return name == ".git" || name == ".workspaces" || name == ".project-trust" ||
           name == ".agent-workspace" || name == "repaired-project" ||
           name.starts_with("repaired-project.tmp.");
}

[[nodiscard]] Result<std::filesystem::path>
safeRelativePath(const std::filesystem::path& relativePath) {
    const auto text = relativePath.generic_string();
    if (!PathGuard::isSafeArchiveEntry(text) || text.size() > 4096U ||
        std::any_of(text.begin(), text.end(), [](unsigned char ch) {
            return ch < 0x20U || ch == 0x7FU;
        })) {
        return Result<std::filesystem::path>::failure("工作区相对路径无效");
    }
    return Result<std::filesystem::path>::success(relativePath.lexically_normal());
}

[[nodiscard]] bool textLooksBinary(std::string_view text) {
    if (text.find('\0') != std::string_view::npos) {
        return true;
    }
    std::size_t controls = 0U;
    for (const auto character : text) {
        const auto value = static_cast<unsigned char>(character);
        if (value < 0x09U || (value > 0x0DU && value < 0x20U)) {
            ++controls;
        }
    }
    return !text.empty() && controls * 20U > text.size();
}

[[nodiscard]] bool containsSecret(std::string text) {
    text = util::lowerAscii(std::move(text));
    constexpr std::string_view markers[]{
        "-----begin private key", "-----begin rsa private key", "client_secret",
        "api_key=",               "apikey=",                    "access_token",
        "refresh_token",          "aws_secret_access_key",      "password="};
    return std::any_of(std::begin(markers), std::end(markers), [&](std::string_view marker) {
        return text.find(marker) != std::string::npos;
    });
}

[[nodiscard]] Result<void> validateTextContent(const std::string& content) {
    if (content.size() > kMaximumEditableBytes) {
        return Result<void>::failure("文本修改超过 1 MiB 上限");
    }
    if (textLooksBinary(content)) {
        return Result<void>::failure("修改内容包含二进制控制字节");
    }
    if (containsSecret(content)) {
        return Result<void>::failure("修改内容包含疑似凭据或密钥");
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void> validateEditableFile(const std::filesystem::path& root,
                                                const std::filesystem::path& file) {
    if (SensitiveFileDetector{}.isSensitive(file)) {
        return Result<void>::failure("敏感文件不允许进入智能体编辑上下文");
    }
    const auto asset = FormatDetector{}.detect(root, file);
    if (!asset.auditable ||
        !(asset.mime.starts_with("text/") || isLikelyTextExtension(asset.extension) ||
          isCodeExtension(asset.extension))) {
        return Result<void>::failure("当前文件不是可安全结构化编辑的文本或源码");
    }
    if (asset.sizeBytes > kMaximumEditableBytes) {
        return Result<void>::failure("待编辑文件超过 1 MiB 上限");
    }
    return Result<void>::success();
}

[[nodiscard]] std::filesystem::path originalFile(const std::filesystem::path& projectRoot,
                                                 const std::filesystem::path& relativePath) {
    std::error_code error;
    if (std::filesystem::is_regular_file(projectRoot, error)) {
        return relativePath == projectRoot.filename() ? projectRoot : std::filesystem::path{};
    }
    return projectRoot / relativePath;
}

[[nodiscard]] Result<void> copyProject(const std::filesystem::path& source,
                                       const std::filesystem::path& destination,
                                       const ImportLimits& limits) {
    std::error_code error;
    std::filesystem::create_directories(destination, error);
    if (error) {
        return Result<void>::failure("无法创建 repaired project: " + error.message());
    }
    if (std::filesystem::is_regular_file(source, error)) {
        const auto size = std::filesystem::file_size(source, error);
        if (error || size > limits.maxSingleFileBytes || size > limits.maxTotalBytes) {
            return Result<void>::failure("项目文件超过 repaired project 复制预算");
        }
        std::filesystem::copy_file(source, destination / source.filename(),
                                   std::filesystem::copy_options::none, error);
        return error ? Result<void>::failure("复制项目文件失败: " + error.message())
                     : Result<void>::success();
    }
    if (!std::filesystem::is_directory(source, error)) {
        return Result<void>::failure("项目副本不存在或不是普通文件/目录");
    }

    std::size_t fileCount = 0U;
    std::uint64_t totalBytes = 0U;
    for (std::filesystem::recursive_directory_iterator iterator(source, error), end;
         iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_directory(error) &&
            PathGuard::isInsideRoot(destination, iterator->path())) {
            iterator.disable_recursion_pending();
            continue;
        }
        if (iterator->is_symlink(error)) {
            if (iterator->is_directory(error)) {
                iterator.disable_recursion_pending();
            }
            continue;
        }
        const auto relative = std::filesystem::relative(iterator->path(), source, error);
        if (error) {
            return Result<void>::failure("计算 repaired project 路径失败: " + error.message());
        }
        if (iterator->is_directory(error)) {
            if (skippedDirectory(relative)) {
                iterator.disable_recursion_pending();
                continue;
            }
            std::filesystem::create_directories(destination / relative, error);
            if (error) {
                return Result<void>::failure("创建 repaired project 子目录失败: " +
                                             error.message());
            }
            continue;
        }
        if (!iterator->is_regular_file(error)) {
            continue;
        }
        if (++fileCount > limits.maxFileCount) {
            return Result<void>::failure("repaired project 文件数量超过预算");
        }
        const auto size = std::filesystem::file_size(iterator->path(), error);
        if (error || size > limits.maxSingleFileBytes) {
            return Result<void>::failure("repaired project 单文件超过预算");
        }
        const auto size64 = static_cast<std::uint64_t>(size);
        if (size64 > limits.maxTotalBytes - totalBytes) {
            return Result<void>::failure("repaired project 总量超过预算");
        }
        totalBytes += size64;
        const auto target = destination / relative;
        if (!PathGuard::isInsideRoot(destination, target)) {
            return Result<void>::failure("repaired project 目标越过工作区边界");
        }
        std::filesystem::create_directories(target.parent_path(), error);
        std::filesystem::copy_file(iterator->path(), target,
                                   std::filesystem::copy_options::none, error);
        if (error) {
            return Result<void>::failure("复制 repaired project 文件失败: " + error.message());
        }
    }
    return error ? Result<void>::failure("遍历项目副本失败: " + error.message())
                 : Result<void>::success();
}

[[nodiscard]] std::string quoteGitPath(const std::filesystem::path& path) {
    std::string quoted{"\""};
    for (const auto character : path.generic_string()) {
        switch (character) {
        case '\\':
            quoted += "\\\\";
            break;
        case '"':
            quoted += "\\\"";
            break;
        case '\t':
            quoted += "\\t";
            break;
        case '\n':
            quoted += "\\n";
            break;
        case '\r':
            quoted += "\\r";
            break;
        default:
            quoted.push_back(character);
            break;
        }
    }
    quoted.push_back('"');
    return quoted;
}

[[nodiscard]] std::vector<std::string> lines(const std::string& text) {
    return util::splitLines(text);
}

[[nodiscard]] std::string range(std::size_t count, char prefix) {
    std::ostringstream output;
    output << prefix << (count == 0U ? 0U : 1U) << ',' << count;
    return output.str();
}

void appendLines(std::ostringstream& output, char prefix, const std::string& text) {
    const auto split = lines(text);
    for (std::size_t index = 0U; index < split.size(); ++index) {
        output << prefix << split[index] << '\n';
        if (index + 1U == split.size() && !text.empty() && !text.ends_with('\n')) {
            output << "\\ No newline at end of file\n";
        }
    }
}

[[nodiscard]] std::string fullFileDiff(const std::filesystem::path& relative,
                                       const std::string* oldText,
                                       const std::string* newText) {
    const auto oldCount = oldText == nullptr ? 0U : lines(*oldText).size();
    const auto newCount = newText == nullptr ? 0U : lines(*newText).size();
    const auto aPath = quoteGitPath(std::filesystem::path{"a"} / relative);
    const auto bPath = quoteGitPath(std::filesystem::path{"b"} / relative);
    std::ostringstream output;
    output << "diff --git " << aPath << ' ' << bPath << '\n';
    if (oldText == nullptr) {
        output << "new file mode 100644\n--- /dev/null\n+++ " << bPath << '\n';
    } else if (newText == nullptr) {
        output << "deleted file mode 100644\n--- " << aPath << "\n+++ /dev/null\n";
    } else {
        output << "--- " << aPath << "\n+++ " << bPath << '\n';
    }
    output << "@@ " << range(oldCount, '-') << ' ' << range(newCount, '+') << " @@\n";
    if (oldText != nullptr) {
        appendLines(output, '-', *oldText);
    }
    if (newText != nullptr) {
        appendLines(output, '+', *newText);
    }
    return output.str();
}

[[nodiscard]] Result<std::vector<std::filesystem::path>>
readManifest(const std::filesystem::path& workspaceRoot) {
    const auto path = manifestPath(workspaceRoot);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return Result<std::vector<std::filesystem::path>>::success({});
    }
    const auto content = util::readFileLimited(path, 256U * 1024U);
    const auto parsed = parseJson(content);
    if (!parsed.ok() || !parsed.value().isArray() ||
        parsed.value().asArray().size() > kMaximumChangedFiles) {
        return Result<std::vector<std::filesystem::path>>::failure(
            "工作区变更清单损坏或超过上限");
    }
    std::vector<std::filesystem::path> result;
    for (const auto& item : parsed.value().asArray()) {
        if (!item.isString()) {
            return Result<std::vector<std::filesystem::path>>::failure("工作区变更路径类型无效");
        }
        const auto checked = safeRelativePath(item.asString());
        if (!checked.ok()) {
            return Result<std::vector<std::filesystem::path>>::failure(checked.error());
        }
        result.push_back(checked.value());
    }
    return Result<std::vector<std::filesystem::path>>::success(std::move(result));
}

[[nodiscard]] Result<void> recordChanged(const std::filesystem::path& workspaceRoot,
                                         const std::filesystem::path& relative) {
    auto manifest = readManifest(workspaceRoot);
    if (!manifest.ok()) {
        return Result<void>::failure(manifest.error());
    }
    if (std::find(manifest.value().begin(), manifest.value().end(), relative) ==
        manifest.value().end()) {
        if (manifest.value().size() >= kMaximumChangedFiles) {
            return Result<void>::failure("工作区变更文件数量超过上限");
        }
        manifest.value().push_back(relative);
    }
    std::sort(manifest.value().begin(), manifest.value().end());
    JsonValue::Array array;
    for (const auto& item : manifest.value()) {
        array.emplace_back(item.generic_string());
    }
    return util::writeTextFile(manifestPath(workspaceRoot), writeJson(JsonValue{array}, 2) + "\n");
}

struct MetadataSnapshot {
    bool hadManifest{false};
    bool hadPatch{false};
    std::string manifest;
    std::string patch;
};

[[nodiscard]] Result<MetadataSnapshot>
snapshotMetadata(const std::filesystem::path& workspaceRoot) {
    MetadataSnapshot snapshot;
    std::error_code error;
    const auto manifest = manifestPath(workspaceRoot);
    snapshot.hadManifest = std::filesystem::is_regular_file(manifest, error);
    if (snapshot.hadManifest) {
        snapshot.manifest = util::readFileLimited(manifest, 256U * 1024U + 1U);
        if (snapshot.manifest.size() > 256U * 1024U) {
            return Result<MetadataSnapshot>::failure("工作区变更清单超过上限");
        }
    }
    const auto patch = patchPath(workspaceRoot);
    snapshot.hadPatch = std::filesystem::is_regular_file(patch, error);
    if (snapshot.hadPatch) {
        snapshot.patch = util::readFileLimited(patch, kMaximumPatchBytes + 1U);
        if (snapshot.patch.size() > kMaximumPatchBytes) {
            return Result<MetadataSnapshot>::failure("工作区补丁超过上限");
        }
    }
    return Result<MetadataSnapshot>::success(std::move(snapshot));
}

[[nodiscard]] Result<void> restoreMetadata(const std::filesystem::path& workspaceRoot,
                                           const MetadataSnapshot& snapshot) {
    std::error_code error;
    if (snapshot.hadManifest) {
        const auto restored = util::writeTextFile(manifestPath(workspaceRoot), snapshot.manifest);
        if (!restored.ok()) {
            return restored;
        }
    } else {
        std::filesystem::remove(manifestPath(workspaceRoot), error);
        if (error) {
            return Result<void>::failure("无法回滚工作区变更清单: " + error.message());
        }
    }
    if (snapshot.hadPatch) {
        return util::writeTextFile(patchPath(workspaceRoot), snapshot.patch);
    }
    std::filesystem::remove(patchPath(workspaceRoot), error);
    return error ? Result<void>::failure("无法回滚工作区补丁: " + error.message())
                 : Result<void>::success();
}

[[nodiscard]] Result<std::string>
combinedPatch(const std::vector<WorkspaceChange>& changes) {
    std::string combined;
    for (const auto& change : changes) {
        if (change.diff.size() > kMaximumPatchBytes - combined.size()) {
            return Result<std::string>::failure("统一补丁超过 16 MiB 上限");
        }
        combined += change.diff;
    }
    return Result<std::string>::success(std::move(combined));
}

[[nodiscard]] std::size_t occurrenceCount(std::string_view text, std::string_view needle) {
    std::size_t count = 0U;
    std::size_t offset = 0U;
    while (!needle.empty() && (offset = text.find(needle, offset)) != std::string_view::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

[[nodiscard]] std::string replaceOccurrences(std::string text, std::string_view expected,
                                             std::string_view replacement) {
    std::size_t offset = 0U;
    while ((offset = text.find(expected, offset)) != std::string::npos) {
        text.replace(offset, expected.size(), replacement);
        offset += replacement.size();
    }
    return text;
}

[[nodiscard]] std::string preview(const std::string& text) {
    constexpr std::size_t kPreviewBytes = 4000U;
    return text.size() <= kPreviewBytes ? text : text.substr(0U, kPreviewBytes) + "\n...[已截断]";
}

} // namespace

WorkspaceEditor::WorkspaceEditor(ImportLimits limits) : limits_{limits} {}

Result<std::filesystem::path>
WorkspaceEditor::prepare(const std::filesystem::path& projectRoot,
                         const std::filesystem::path& workspaceRoot) const {
    if (projectRoot.empty() || workspaceRoot.empty()) {
        return Result<std::filesystem::path>::failure("缺少项目副本或工作区路径");
    }
    const auto destination = repairedRoot(workspaceRoot);
    std::error_code error;
    if (std::filesystem::is_directory(destination, error)) {
        return Result<std::filesystem::path>::success(destination);
    }
    std::filesystem::create_directories(workspaceRoot, error);
    if (error) {
        return Result<std::filesystem::path>::failure("无法创建智能体工作区: " + error.message());
    }
    auto temporary = workspaceRoot / ("repaired-project.tmp." + util::makeSessionId());
    const auto copied = copyProject(projectRoot, temporary, limits_);
    if (!copied.ok()) {
        std::filesystem::remove_all(temporary, error);
        return Result<std::filesystem::path>::failure(copied.error());
    }
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove_all(temporary, error);
        if (std::filesystem::is_directory(destination, error)) {
            return Result<std::filesystem::path>::success(destination);
        }
        return Result<std::filesystem::path>::failure("提交 repaired project 失败: " +
                                                       error.message());
    }
    return Result<std::filesystem::path>::success(destination);
}

Result<WorkspaceEditResult>
WorkspaceEditor::applyTextEdit(const std::filesystem::path& projectRoot,
                               const std::filesystem::path& workspaceRoot,
                               const std::filesystem::path& relativePath,
                               const std::string& expectedText,
                               const std::string& replacementText,
                               std::size_t expectedOccurrences) const {
    const auto relative = safeRelativePath(relativePath);
    if (!relative.ok()) {
        return Result<WorkspaceEditResult>::failure(relative.error());
    }
    if (expectedText.empty() || expectedOccurrences == 0U || expectedOccurrences > 100U) {
        return Result<WorkspaceEditResult>::failure("精确编辑必须提供非空锚点和 1..100 的命中数");
    }
    const auto replacementValid = validateTextContent(replacementText);
    if (!replacementValid.ok()) {
        return Result<WorkspaceEditResult>::failure(replacementValid.error());
    }
    const auto prepared = prepare(projectRoot, workspaceRoot);
    if (!prepared.ok()) {
        return Result<WorkspaceEditResult>::failure(prepared.error());
    }
    const auto target = prepared.value() / relative.value();
    if (!PathGuard::isInsideRoot(prepared.value(), target)) {
        return Result<WorkspaceEditResult>::failure("编辑目标越过 repaired project 边界");
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(target, error)) {
        return Result<WorkspaceEditResult>::failure("待编辑文件不存在");
    }
    const auto editable = validateEditableFile(prepared.value(), target);
    if (!editable.ok()) {
        return Result<WorkspaceEditResult>::failure(editable.error());
    }
    const auto oldText = util::readFileLimited(target, kMaximumEditableBytes + 1U);
    if (oldText.size() > kMaximumEditableBytes) {
        return Result<WorkspaceEditResult>::failure("待编辑文件超过读取上限");
    }
    const auto actualOccurrences = occurrenceCount(oldText, expectedText);
    if (actualOccurrences != expectedOccurrences) {
        return Result<WorkspaceEditResult>::failure(
            "精确编辑锚点命中数不符：期望 " + std::to_string(expectedOccurrences) +
            "，实际 " + std::to_string(actualOccurrences));
    }
    const auto removedBytes = expectedText.size() * actualOccurrences;
    const auto addedBytes = replacementText.size() * actualOccurrences;
    if (removedBytes > oldText.size() || addedBytes > kMaximumEditableBytes ||
        oldText.size() - removedBytes > kMaximumEditableBytes - addedBytes) {
        return Result<WorkspaceEditResult>::failure("精确编辑后的文件将超过 1 MiB 上限");
    }
    const auto newText = replaceOccurrences(oldText, expectedText, replacementText);
    if (newText == oldText) {
        return Result<WorkspaceEditResult>::failure("修改前后内容相同，未生成无意义变更");
    }
    const auto resultValid = validateTextContent(newText);
    if (!resultValid.ok()) {
        return Result<WorkspaceEditResult>::failure(resultValid.error());
    }
    const auto metadata = snapshotMetadata(workspaceRoot);
    if (!metadata.ok()) {
        return Result<WorkspaceEditResult>::failure(metadata.error());
    }
    const auto rollback = [&](const std::string& reason) {
        std::string errorMessage = reason;
        const auto fileRestored = util::writeTextFile(target, oldText);
        if (!fileRestored.ok()) {
            errorMessage += "；文件回滚失败: " + fileRestored.error();
        }
        const auto metadataRestored = restoreMetadata(workspaceRoot, metadata.value());
        if (!metadataRestored.ok()) {
            errorMessage += "；元数据回滚失败: " + metadataRestored.error();
        }
        return Result<WorkspaceEditResult>::failure(std::move(errorMessage));
    };
    const auto written = util::writeTextFile(target, newText);
    if (!written.ok()) {
        return Result<WorkspaceEditResult>::failure(written.error());
    }
    const auto recorded = recordChanged(workspaceRoot, relative.value());
    if (!recorded.ok()) {
        return rollback(recorded.error());
    }
    const auto allChanges = changes(projectRoot, workspaceRoot);
    if (!allChanges.ok()) {
        return rollback(allChanges.error());
    }
    const auto combined = combinedPatch(allChanges.value());
    if (!combined.ok()) {
        return rollback(combined.error());
    }
    const auto patch = util::writeTextFile(patchPath(workspaceRoot), combined.value());
    if (!patch.ok()) {
        return rollback(patch.error());
    }
    return Result<WorkspaceEditResult>::success(
        {.repairedRoot = prepared.value(),
         .relativePath = relative.value(),
         .patchFile = patchPath(workspaceRoot),
         .diff = fullFileDiff(relative.value(), &oldText, &newText),
         .preview = preview(newText)});
}

Result<WorkspaceEditResult>
WorkspaceEditor::createTextFile(const std::filesystem::path& projectRoot,
                                const std::filesystem::path& workspaceRoot,
                                const std::filesystem::path& relativePath,
                                const std::string& content) const {
    const auto relative = safeRelativePath(relativePath);
    if (!relative.ok()) {
        return Result<WorkspaceEditResult>::failure(relative.error());
    }
    if (content.empty()) {
        return Result<WorkspaceEditResult>::failure("新建文本文件内容不能为空");
    }
    const auto contentValid = validateTextContent(content);
    if (!contentValid.ok()) {
        return Result<WorkspaceEditResult>::failure(contentValid.error());
    }
    const auto prepared = prepare(projectRoot, workspaceRoot);
    if (!prepared.ok()) {
        return Result<WorkspaceEditResult>::failure(prepared.error());
    }
    const auto target = prepared.value() / relative.value();
    if (!PathGuard::isInsideRoot(prepared.value(), target) ||
        SensitiveFileDetector{}.isSensitive(target)) {
        return Result<WorkspaceEditResult>::failure("新文件路径未通过安全策略");
    }
    std::error_code error;
    if (std::filesystem::exists(target, error)) {
        return Result<WorkspaceEditResult>::failure("目标文件已存在，请使用精确编辑工具");
    }
    const auto extension = util::lowerAscii(target.extension().generic_string());
    if (!(isLikelyTextExtension(extension) || isCodeExtension(extension) || extension.empty())) {
        return Result<WorkspaceEditResult>::failure("只允许创建文本、源码或配置文件");
    }
    const auto metadata = snapshotMetadata(workspaceRoot);
    if (!metadata.ok()) {
        return Result<WorkspaceEditResult>::failure(metadata.error());
    }
    const auto rollback = [&](const std::string& reason) {
        std::string errorMessage = reason;
        std::filesystem::remove(target, error);
        if (error) {
            errorMessage += "；新文件回滚失败: " + error.message();
            error.clear();
        }
        const auto metadataRestored = restoreMetadata(workspaceRoot, metadata.value());
        if (!metadataRestored.ok()) {
            errorMessage += "；元数据回滚失败: " + metadataRestored.error();
        }
        return Result<WorkspaceEditResult>::failure(std::move(errorMessage));
    };
    const auto written = util::writeTextFile(target, content);
    if (!written.ok()) {
        return Result<WorkspaceEditResult>::failure(written.error());
    }
    const auto recorded = recordChanged(workspaceRoot, relative.value());
    if (!recorded.ok()) {
        return rollback(recorded.error());
    }
    const auto allChanges = changes(projectRoot, workspaceRoot);
    if (!allChanges.ok()) {
        return rollback(allChanges.error());
    }
    const auto combined = combinedPatch(allChanges.value());
    if (!combined.ok()) {
        return rollback(combined.error());
    }
    const auto patch = util::writeTextFile(patchPath(workspaceRoot), combined.value());
    if (!patch.ok()) {
        return rollback(patch.error());
    }
    return Result<WorkspaceEditResult>::success(
        {.repairedRoot = prepared.value(),
         .relativePath = relative.value(),
         .patchFile = patchPath(workspaceRoot),
         .diff = fullFileDiff(relative.value(), nullptr, &content),
         .preview = preview(content)});
}

Result<std::string>
WorkspaceEditor::readTextFile(const std::filesystem::path& projectRoot,
                              const std::filesystem::path& workspaceRoot,
                              const std::filesystem::path& relativePath,
                              std::size_t maxBytes) const {
    const auto relative = safeRelativePath(relativePath);
    if (!relative.ok()) {
        return Result<std::string>::failure(relative.error());
    }
    if (maxBytes == 0U || maxBytes > 64U * 1024U) {
        return Result<std::string>::failure("工作区读取上限必须位于 1..65536 字节");
    }
    const auto prepared = prepare(projectRoot, workspaceRoot);
    if (!prepared.ok()) {
        return Result<std::string>::failure(prepared.error());
    }
    const auto target = prepared.value() / relative.value();
    const auto editable = validateEditableFile(prepared.value(), target);
    if (!editable.ok()) {
        return Result<std::string>::failure(editable.error());
    }
    return Result<std::string>::success(util::readFileLimited(target, maxBytes));
}

Result<std::vector<WorkspaceChange>>
WorkspaceEditor::changes(const std::filesystem::path& projectRoot,
                         const std::filesystem::path& workspaceRoot) const {
    const auto prepared = prepare(projectRoot, workspaceRoot);
    if (!prepared.ok()) {
        return Result<std::vector<WorkspaceChange>>::failure(prepared.error());
    }
    const auto manifest = readManifest(workspaceRoot);
    if (!manifest.ok()) {
        return Result<std::vector<WorkspaceChange>>::failure(manifest.error());
    }
    std::vector<WorkspaceChange> result;
    for (const auto& relative : manifest.value()) {
        const auto oldFile = originalFile(projectRoot, relative);
        const auto newFile = prepared.value() / relative;
        std::error_code error;
        const auto hasOld = !oldFile.empty() && std::filesystem::is_regular_file(oldFile, error);
        const auto hasNew = std::filesystem::is_regular_file(newFile, error);
        if (!hasOld && !hasNew) {
            continue;
        }
        const auto oldText = hasOld ? util::readFileLimited(oldFile, kMaximumEditableBytes + 1U)
                                    : std::string{};
        const auto newText = hasNew ? util::readFileLimited(newFile, kMaximumEditableBytes + 1U)
                                    : std::string{};
        if ((hasOld && oldText.size() > kMaximumEditableBytes) ||
            (hasNew && newText.size() > kMaximumEditableBytes)) {
            return Result<std::vector<WorkspaceChange>>::failure(
                "变更文件超过可生成 diff 的上限: " + relative.generic_string());
        }
        if (hasOld && hasNew && oldText == newText) {
            continue;
        }
        const auto* oldPointer = hasOld ? &oldText : nullptr;
        const auto* newPointer = hasNew ? &newText : nullptr;
        result.push_back({.relativePath = relative,
                          .kind = !hasOld ? "created" : (!hasNew ? "deleted" : "modified"),
                          .diff = fullFileDiff(relative, oldPointer, newPointer)});
    }
    return Result<std::vector<WorkspaceChange>>::success(std::move(result));
}

Result<AuditResult> WorkspaceEditor::reAudit(const std::filesystem::path& projectRoot,
                                             const std::filesystem::path& workspaceRoot,
                                             const AuditOptions& options,
                                             const ProjectContext* baselineContext) const {
    const auto prepared = prepare(projectRoot, workspaceRoot);
    if (!prepared.ok()) {
        return Result<AuditResult>::failure(prepared.error());
    }
    auto manifest = readManifest(workspaceRoot);
    if (!manifest.ok()) {
        return Result<AuditResult>::failure(manifest.error());
    }
    auto effectiveOptions = options;
    effectiveOptions.unverifiedFiles.insert(effectiveOptions.unverifiedFiles.end(),
                                            manifest.value().begin(), manifest.value().end());
    std::sort(effectiveOptions.unverifiedFiles.begin(), effectiveOptions.unverifiedFiles.end());
    effectiveOptions.unverifiedFiles.erase(
        std::unique(effectiveOptions.unverifiedFiles.begin(),
                    effectiveOptions.unverifiedFiles.end()),
        effectiveOptions.unverifiedFiles.end());

    ProjectContext context = baselineContext == nullptr ? ProjectContext{} : *baselineContext;
    context.originalRoot = baselineContext == nullptr ? projectRoot : baselineContext->originalRoot;
    context.inputRoot = prepared.value();
    if (baselineContext == nullptr) {
        context.workspaceRoot = workspaceRoot.filename() == "agent" ? workspaceRoot.parent_path()
                                                                     : workspaceRoot;
    }
    context.sessionId = "repaired-" + util::makeSessionId();
    if (context.projectName.empty()) {
        context.projectName = projectRoot.stem().generic_string();
    }
    context.unpackStatus = "REPAIRED_WORKSPACE";
    context.warnings.push_back(
        "repaired-project 中的修改文件按待人工确认草稿处理，不会充当独立证据");
    return AuditEngine{}.run(context, effectiveOptions);
}

} // namespace cc
