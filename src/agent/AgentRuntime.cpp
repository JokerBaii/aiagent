/**
 * @file AgentRuntime.cpp
 * @brief 受控智能体运行时实现。
 */

#include "cc/agent/AgentRuntime.hpp"

#include "cc/agent/PermissionGate.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/agent/ToolRegistry.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/LibArchiveReader.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/loader/ZipArchiveReader.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <utility>

namespace cc {
namespace {

constexpr std::size_t kDefaultReadLimit = 12000U;
constexpr std::size_t kPreviewLimit = 1200U;
constexpr std::size_t kSearchReadLimit = 256000U;

[[nodiscard]] std::string pathText(const std::filesystem::path& path) {
    return path.generic_string();
}

[[nodiscard]] std::string truncateText(const std::string& value, std::size_t limit) {
    if (value.size() <= limit) {
        return value;
    }
    return value.substr(0U, limit) + "\n...[已截断]";
}

[[nodiscard]] std::string extensionLower(const std::filesystem::path& path) {
    return util::lowerAscii(path.extension().generic_string());
}

[[nodiscard]] bool isReadableTextLike(const std::filesystem::path& path) {
    const auto ext = extensionLower(path);
    return ext.empty() || isLikelyTextExtension(ext) || isCodeExtension(ext);
}

[[nodiscard]] std::filesystem::path readableRoot(const AgentRunRequest& request) {
    return request.projectRoot;
}

[[nodiscard]] std::filesystem::path writableRoot(const AgentRunRequest& request) {
    if (!request.workspaceRoot.empty()) {
        return request.workspaceRoot;
    }
    const auto root = readableRoot(request);
    return root / ".project-trust" / "agent-workspace";
}

[[nodiscard]] Result<std::filesystem::path> resolveProjectPath(const AgentRunRequest& request,
                                                               const JsonValue& input) {
    const auto relative = input.at("path").asString();
    const auto root = readableRoot(request);
    std::error_code ec;
    if (std::filesystem::is_regular_file(root, ec)) {
        const auto selectedName = root.filename().generic_string();
        if (relative.empty() || relative == "." || relative == "./" || relative == selectedName) {
            return PathGuard::normalize(root);
        }
        return Result<std::filesystem::path>::failure("当前项目输入是单个文件，只能读取该文件: " +
                                                      selectedName);
    }
    if (relative.empty()) {
        return Result<std::filesystem::path>::failure("工具调用缺少 path");
    }
    if (!PathGuard::isSafeArchiveEntry(relative)) {
        return Result<std::filesystem::path>::failure("拒绝读取不安全相对路径: " + relative);
    }
    const auto target = root / relative;
    if (!PathGuard::isInsideRoot(root, target)) {
        return Result<std::filesystem::path>::failure("拒绝读取项目边界外路径: " + relative);
    }
    return PathGuard::normalize(target);
}

[[nodiscard]] Result<std::filesystem::path> resolveWorkspacePath(const AgentRunRequest& request,
                                                                 const JsonValue& input) {
    const auto relative = input.at("path").asString();
    if (relative.empty()) {
        return Result<std::filesystem::path>::failure("工具调用缺少 path");
    }
    if (!PathGuard::isSafeArchiveEntry(relative)) {
        return Result<std::filesystem::path>::failure("拒绝写入不安全工作区路径: " + relative);
    }
    const auto root = writableRoot(request);
    const auto target = root / relative;
    if (!PathGuard::isInsideRoot(root, target)) {
        return Result<std::filesystem::path>::failure("拒绝写入工作区边界外路径: " + relative);
    }
    return PathGuard::normalize(target);
}

[[nodiscard]] AgentToolCall call(std::string id, std::string name, std::string reason,
                                 JsonValue input = JsonValue::Object{}) {
    return AgentToolCall{.id = std::move(id),
                         .name = std::move(name),
                         .reason = std::move(reason),
                         .input = std::move(input)};
}

[[nodiscard]] std::string permissionDeniedText(ToolPermission permission) {
    return "权限门控拒绝工具调用，需要权限: " + toString(permission);
}

[[nodiscard]] JsonValue assetJson(const ProjectAsset& asset) {
    return JsonValue::Object{{"path", asset.relativePath.generic_string()},
                             {"name", asset.fileName},
                             {"extension", asset.extension},
                             {"size_bytes", static_cast<double>(asset.sizeBytes)},
                             {"format", asset.format},
                             {"mime", asset.mime},
                             {"language", asset.language},
                             {"text_readable", isReadableTextLike(asset.absolutePath)},
                             {"auditable", asset.auditable}};
}

[[nodiscard]] ProjectAsset detectProjectAsset(const AgentRunRequest& request,
                                              const std::filesystem::path& file) {
    const auto root = readableRoot(request);
    std::error_code ec;
    const auto base = std::filesystem::is_regular_file(root, ec) ? root.parent_path() : root;
    return FormatDetector{}.detect(base.empty() ? root : base, file);
}

[[nodiscard]] bool shouldSkipDirectory(const std::filesystem::path& path) {
    const auto name = path.filename().generic_string();
    return name == ".git" || name == ".workspaces" || name == ".project-trust" ||
           name == ".agent-workspace";
}

[[nodiscard]] const AgentToolSpec* findSpec(const std::string& name,
                                            const std::vector<AgentToolSpec>& specs) {
    const auto iter = std::find_if(specs.begin(), specs.end(),
                                   [&](const AgentToolSpec& spec) { return spec.name == name; });
    return iter == specs.end() ? nullptr : &(*iter);
}

[[nodiscard]] JsonValue observationJson(const AgentObservation& observation) {
    return JsonValue::Object{{"call_id", observation.callId},
                             {"tool_name", observation.toolName},
                             {"ok", observation.ok},
                             {"summary", observation.summary},
                             {"output", observation.output}};
}

[[nodiscard]] JsonValue callJson(const AgentToolCall& callItem) {
    return JsonValue::Object{{"id", callItem.id},
                             {"name", callItem.name},
                             {"reason", callItem.reason},
                             {"input", callItem.input}};
}

[[nodiscard]] JsonValue traceJson(const AgentPlan& plan,
                                  const std::vector<AgentObservation>& observations,
                                  const std::vector<AgentEvent>& events,
                                  const std::string& finalAnswer) {
    JsonValue::Array callArray;
    for (const auto& item : plan.calls) {
        callArray.push_back(callJson(item));
    }
    JsonValue::Array observationArray;
    for (const auto& observation : observations) {
        observationArray.push_back(observationJson(observation));
    }
    JsonValue::Array eventArray;
    for (const auto& item : events) {
        eventArray.push_back(JsonValue::Object{{"kind", toString(item.kind)},
                                               {"role", item.role},
                                               {"text", item.text},
                                               {"context", item.context},
                                               {"payload", item.payload}});
    }
    return JsonValue::Object{
        {"plan", JsonValue::Object{{"summary", plan.summary}, {"calls", JsonValue{callArray}}}},
        {"observations", JsonValue{observationArray}},
        {"events", JsonValue{eventArray}},
        {"final_answer", finalAnswer}};
}

[[nodiscard]] std::string auditSummary(const AuditResult& result) {
    std::ostringstream output;
    output << "当前材料可信评分 " << result.trustScore.totalScore << "/100，还有 "
           << result.trustScore.trustDebt << " 分需要通过补材料、补证据或修正矛盾来恢复。";
    if (!result.findings.empty()) {
        output << "首个风险：" << result.findings.front().title << "，原因："
               << result.findings.front().reason << "。";
    }
    if (!result.fixTasks.empty()) {
        output << "优先补证：" << result.fixTasks.front().title << "。";
    }
    output << "评分来自可复核的材料规则和证据匹配，不由大模型随意改动。";
    return output.str();
}

[[nodiscard]] std::string normalizeMarkdown(const std::string& markdown) {
    const auto lines = util::splitLines(markdown);
    std::ostringstream output;
    bool previousBlank = false;
    for (const auto& rawLine : lines) {
        std::string line = rawLine;
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())) != 0) {
            line.pop_back();
        }

        if (!line.empty() && line.front() == '#') {
            std::size_t index = 0U;
            while (index < line.size() && line[index] == '#') {
                ++index;
            }
            if (index < line.size() && line[index] != ' ') {
                line.insert(index, " ");
            }
        }

        const bool blank = util::trim(line).empty();
        if (blank && previousBlank) {
            continue;
        }
        output << line << '\n';
        previousBlank = blank;
    }
    return output.str();
}

[[nodiscard]] std::string
finalAnswerFromObservations(const AgentRunRequest& request,
                            const std::vector<AgentObservation>& observations) {
    std::ostringstream output;
    output << "未启用 LLM Brain，本地运行时只完成了可复核的上下文收集，"
              "没有冒充模型做语义判断或给出最终方案。";
    if (request.auditResult != nullptr) {
        output << " 已把当前审计结论作为只读上下文，没有改写评分。";
    }
    for (const auto& observation : observations) {
        output << "\n- " << observation.summary;
    }
    return output.str();
}

[[nodiscard]] AgentEvent planEvent(const AgentPlan& plan, const std::string& planner) {
    JsonValue::Array calls;
    for (const auto& item : plan.calls) {
        calls.push_back(callJson(item));
    }
    return AgentEvent{.kind = AgentEventKind::Plan,
                      .role = "计划",
                      .text = plan.summary,
                      .context = planner,
                      .payload = JsonValue::Object{{"calls", JsonValue{calls}}}};
}

[[nodiscard]] AgentEvent toolEvent(const AgentObservation& observation) {
    return AgentEvent{.kind = AgentEventKind::Tool,
                      .role = "工具",
                      .text = observation.summary,
                      .context = observation.toolName,
                      .payload = observationJson(observation)};
}

[[nodiscard]] AgentEvent assistantEvent(const std::string& finalAnswer) {
    return AgentEvent{.kind = AgentEventKind::Assistant,
                      .role = "智能体",
                      .text = finalAnswer,
                      .context = "已记录 trace",
                      .payload = JsonValue::Object{}};
}

[[nodiscard]] std::vector<AgentEvent> buildEvents(const AgentPlan& plan, const std::string& planner,
                                                  const std::vector<AgentObservation>& observations,
                                                  const std::string& finalAnswer) {
    std::vector<AgentEvent> events;
    events.push_back(planEvent(plan, planner));
    for (const auto& observation : observations) {
        events.push_back(toolEvent(observation));
    }
    events.push_back(assistantEvent(finalAnswer));
    return events;
}

[[nodiscard]] std::string firstReadablePath(const AgentObservation& observation) {
    if (!observation.ok) {
        return {};
    }
    for (const auto& item : observation.output.at("files").asArray()) {
        if (item.isString()) {
            const auto path = item.asString();
            if (isReadableTextLike(path)) {
                return path;
            }
            continue;
        }
        const auto path = item.at("path").asString();
        if (item.at("text_readable").asBool(false) && !path.empty()) {
            return path;
        }
    }
    return {};
}

[[nodiscard]] Result<AgentObservation> runSummarizeAuditSession(const AgentRunRequest& request,
                                                                const AgentToolCall& callItem) {
    (void)callItem;
    if (request.auditResult == nullptr) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "当前还没有审计结果可总结",
                             .output = JsonValue::Object{}});
    }
    const auto summary = auditSummary(*request.auditResult);
    return Result<AgentObservation>::success(
        AgentObservation{.callId = callItem.id,
                         .toolName = callItem.name,
                         .ok = true,
                         .summary = "已读取审计会话摘要",
                         .output = JsonValue::Object{{"summary", summary}}});
}

[[nodiscard]] Result<AgentToolExecution>
runProjectAuditExecution(const AgentRunRequest& request, const AgentToolCall& callItem,
                         const AgentObservationObserver& observe) {
    if (request.auditResult != nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{.callId = callItem.id,
                                              .toolName = callItem.name,
                                              .ok = false,
                                              .summary = "当前会话已有审计结果，无需重复运行；请读取摘要或继续核查文件",
                                              .output = JsonValue::Object{}}},
            .auditResult = std::nullopt});
    }
    if (request.projectRoot.empty()) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{.callId = callItem.id,
                                              .toolName = callItem.name,
                                              .ok = false,
                                              .summary = "未提供可审计的项目路径",
                                              .output = JsonValue::Object{}}},
            .auditResult = std::nullopt});
    }

    StagedAuditPipeline pipeline;
    auto begun = pipeline.begin(request.projectRoot, request.auditOptions);
    if (!begun.ok()) {
        return Result<AgentToolExecution>::failure(begun.error());
    }

    std::vector<AgentObservation> observations;
    observations.reserve(StagedAuditPipeline::stages().size() + 1U);
    while (pipeline.hasNext()) {
        auto observed = pipeline.advance();
        if (!observed.ok()) {
            return Result<AgentToolExecution>::failure(observed.error());
        }
        observed.value().callId = callItem.id + ":" + observed.value().callId;
        if (observe) {
            observe(observed.value());
        }
        observations.push_back(std::move(observed.value()));
    }

    auto result = pipeline.finish();
    if (!result.ok()) {
        return Result<AgentToolExecution>::failure(result.error());
    }
    const auto summary = auditSummary(result.value());
    AgentObservation completed{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已完成确定性规则审计，结果已交回 Brain 继续研判",
        .output = JsonValue::Object{
            {"summary", summary},
            {"session_id", result.value().context.sessionId},
            {"project_root", pathText(result.value().context.inputRoot)},
            {"workspace_root", pathText(result.value().context.workspaceRoot)},
            {"score", result.value().trustScore.totalScore},
            {"trust_debt", result.value().trustScore.trustDebt},
            {"finding_count", static_cast<int>(result.value().findings.size())},
            {"evidence_match_count", static_cast<int>(result.value().evidenceMatches.size())},
            {"fix_task_count", static_cast<int>(result.value().fixTasks.size())}}};
    if (observe) {
        observe(completed);
    }
    observations.push_back(std::move(completed));
    return Result<AgentToolExecution>::success(
        AgentToolExecution{.observations = std::move(observations),
                           .auditResult = std::move(result.value())});
}

[[nodiscard]] Result<AgentObservation> runListProjectFiles(const AgentRunRequest& request,
                                                           const AgentToolCall& callItem) {
    const auto root = readableRoot(request);
    std::error_code ec;
    if (root.empty() || !std::filesystem::exists(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不存在或不可访问",
                             .output = JsonValue::Object{{"root", pathText(root)}}});
    }

    const auto maxFiles =
        static_cast<std::size_t>(std::max(1.0, callItem.input.at("max_files").asNumber(80.0)));
    JsonValue::Array files;

    if (std::filesystem::is_regular_file(root, ec)) {
        auto asset = detectProjectAsset(request, root);
        asset.relativePath = root.filename();
        files.push_back(assetJson(asset));
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = true,
            .summary = "当前输入是单个项目文件，已读取文件元数据",
            .output = JsonValue::Object{{"root", pathText(root)}, {"files", JsonValue{files}}}});
    }

    if (!std::filesystem::is_directory(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不是可枚举目录或普通文件",
                             .output = JsonValue::Object{{"root", pathText(root)}}});
    }

    for (std::filesystem::recursive_directory_iterator iter(root, ec), end; iter != end && !ec;
         iter.increment(ec)) {
        if (iter->is_directory(ec) && shouldSkipDirectory(iter->path())) {
            iter.disable_recursion_pending();
            continue;
        }
        if (!iter->is_regular_file(ec)) {
            continue;
        }
        const auto relative = std::filesystem::relative(iter->path(), root, ec);
        if (ec) {
            continue;
        }
        auto asset = detectProjectAsset(request, iter->path());
        asset.relativePath = relative;
        files.push_back(assetJson(asset));
        if (files.size() >= maxFiles) {
            break;
        }
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已枚举项目文件 " + std::to_string(files.size()) + " 个",
        .output = JsonValue::Object{{"root", pathText(root)}, {"files", JsonValue{files}}}});
}

[[nodiscard]] Result<AgentObservation> runInspectProjectFile(const AgentRunRequest& request,
                                                             const AgentToolCall& callItem) {
    auto path = resolveProjectPath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = path.error(),
                                                                  .output = JsonValue::Object{}});
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path.value(), ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "目标不是普通文件: " + pathText(path.value()),
                             .output = JsonValue::Object{}});
    }
    const auto asset = detectProjectAsset(request, path.value());
    const bool canReadText = isReadableTextLike(path.value());
    const bool canInspectArchive = ArchiveExtractor::isArchivePath(path.value());
    const auto suggested = canReadText         ? "read_text_file"
                           : canInspectArchive ? "inspect_archive"
                                               : "list_project_files";
    return Result<AgentObservation>::success(
        AgentObservation{.callId = callItem.id,
                         .toolName = callItem.name,
                         .ok = true,
                         .summary = "已检查文件格式: " + pathText(asset.relativePath),
                         .output = JsonValue::Object{{"path", pathText(asset.relativePath)},
                                                     {"asset", assetJson(asset)},
                                                     {"can_read_text", canReadText},
                                                     {"can_inspect_archive", canInspectArchive},
                                                     {"suggested_tool", suggested}}});
}

[[nodiscard]] Result<AgentObservation> runReadTextFile(const AgentRunRequest& request,
                                                       const AgentToolCall& callItem) {
    auto path = resolveProjectPath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = path.error(),
                                                                  .output = JsonValue::Object{}});
    }
    const auto asset = detectProjectAsset(request, path.value());
    if (!isReadableTextLike(path.value())) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "该文件不是安全文本/代码格式: " + pathText(path.value()),
                             .output = JsonValue::Object{{"asset", assetJson(asset)}}});
    }
    const auto limit = static_cast<std::size_t>(
        std::max(1.0, callItem.input.at("max_bytes").asNumber(kDefaultReadLimit)));
    const auto content = util::readFileLimited(path.value(), limit);
    return Result<AgentObservation>::success(
        AgentObservation{.callId = callItem.id,
                         .toolName = callItem.name,
                         .ok = true,
                         .summary = "已读取文本/代码文件: " + callItem.input.at("path").asString(),
                         .output = JsonValue::Object{{"path", callItem.input.at("path").asString()},
                                                     {"asset", assetJson(asset)},
                                                     {"content", content}}});
}

[[nodiscard]] JsonValue archiveEntryJson(const std::filesystem::path& relativePath, bool directory,
                                         bool symlink, std::uint64_t sizeBytes,
                                         std::uint64_t compressedBytes = 0U,
                                         std::uint16_t compressionMethod = 0U) {
    const auto name = relativePath.generic_string();
    return JsonValue::Object{{"path", name},
                             {"directory", directory},
                             {"symlink", symlink},
                             {"size_bytes", static_cast<double>(sizeBytes)},
                             {"compressed_bytes", static_cast<double>(compressedBytes)},
                             {"compression_method", static_cast<int>(compressionMethod)},
                             {"safe_path", PathGuard::isSafeArchiveEntry(name)},
                             {"nested_archive", ArchiveExtractor::isArchivePath(relativePath)}};
}

[[nodiscard]] bool archiveEntrySafe(const JsonValue& entry) {
    return entry.at("safe_path").asBool(false) && !entry.at("symlink").asBool(false) &&
           !entry.at("nested_archive").asBool(false);
}

[[nodiscard]] bool isZipArchivePath(const std::filesystem::path& path) {
    return extensionLower(path) == ".zip";
}

[[nodiscard]] Result<AgentObservation> runInspectArchive(const AgentRunRequest& request,
                                                         const AgentToolCall& callItem) {
    auto path = resolveProjectPath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = path.error(),
                                                                  .output = JsonValue::Object{}});
    }
    const auto asset = detectProjectAsset(request, path.value());
    if (!ArchiveExtractor::isArchivePath(path.value())) {
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = false,
            .summary = "目标不是压缩包或代码包: " + pathText(asset.relativePath),
            .output = JsonValue::Object{{"asset", assetJson(asset)}, {"supported", false}}});
    }
    if (!ArchiveExtractor::isSupportedArchivePath(path.value())) {
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = false,
            .summary = "该压缩格式当前只能识别，不能安全枚举: " + pathText(asset.relativePath),
            .output = JsonValue::Object{{"asset", assetJson(asset)}, {"supported", false}}});
    }

    const auto maxEntries =
        static_cast<std::size_t>(std::max(1.0, callItem.input.at("max_entries").asNumber(120.0)));
    JsonValue::Array entries;
    bool safeToExtract = true;
    bool truncated = false;

    if (isZipArchivePath(path.value())) {
        auto listed = ZipArchiveReader{}.list(path.value());
        if (!listed.ok()) {
            return Result<AgentObservation>::success(AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "zip 目录读取失败: " + listed.error(),
                .output = JsonValue::Object{{"asset", assetJson(asset)}, {"supported", true}}});
        }
        for (const auto& entry : listed.value()) {
            auto item = archiveEntryJson(entry.relativePath, entry.directory, entry.symlink,
                                         entry.uncompressedSize, entry.compressedSize,
                                         entry.compressionMethod);
            safeToExtract = safeToExtract && archiveEntrySafe(item);
            if (entries.size() < maxEntries) {
                entries.push_back(std::move(item));
            } else {
                truncated = true;
            }
        }
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = true,
            .summary = "已枚举 zip 包条目 " + std::to_string(listed.value().size()) + " 个",
            .output = JsonValue::Object{{"path", pathText(asset.relativePath)},
                                        {"asset", assetJson(asset)},
                                        {"supported", true},
                                        {"safe_to_extract", safeToExtract},
                                        {"truncated", truncated},
                                        {"entries", JsonValue{entries}}}});
    }

    auto listed = LibArchiveReader{}.list(path.value());
    if (!listed.ok()) {
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = false,
            .summary = "libarchive 目录读取失败: " + listed.error(),
            .output = JsonValue::Object{{"asset", assetJson(asset)}, {"supported", true}}});
    }
    for (const auto& entry : listed.value()) {
        auto item =
            archiveEntryJson(entry.relativePath, entry.directory, entry.symlink, entry.sizeBytes);
        safeToExtract = safeToExtract && archiveEntrySafe(item);
        if (entries.size() < maxEntries) {
            entries.push_back(std::move(item));
        } else {
            truncated = true;
        }
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已枚举压缩包条目 " + std::to_string(listed.value().size()) + " 个",
        .output = JsonValue::Object{{"path", pathText(asset.relativePath)},
                                    {"asset", assetJson(asset)},
                                    {"supported", true},
                                    {"safe_to_extract", safeToExtract},
                                    {"truncated", truncated},
                                    {"entries", JsonValue{entries}}}});
}

[[nodiscard]] Result<AgentObservation> runSearchProjectText(const AgentRunRequest& request,
                                                            const AgentToolCall& callItem) {
    const auto root = readableRoot(request);
    std::error_code ec;
    const auto query = callItem.input.at("query").asString();
    if (util::trim(query).empty()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = "搜索关键词为空",
                                                                  .output = JsonValue::Object{}});
    }

    const auto maxFiles =
        static_cast<std::size_t>(std::max(1.0, callItem.input.at("max_files").asNumber(80.0)));
    const auto maxMatches =
        static_cast<std::size_t>(std::max(1.0, callItem.input.at("max_matches").asNumber(40.0)));
    const auto caseSensitive = callItem.input.at("case_sensitive").asBool(false);
    const auto needle = caseSensitive ? query : util::lowerAscii(query);

    std::size_t scannedFiles = 0U;
    JsonValue::Array matches;
    if (root.empty() || !std::filesystem::exists(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不存在或不可访问",
                             .output = JsonValue::Object{{"root", pathText(root)}}});
    }

    if (std::filesystem::is_regular_file(root, ec)) {
        if (!isReadableTextLike(root)) {
            return Result<AgentObservation>::success(
                AgentObservation{.callId = callItem.id,
                                 .toolName = callItem.name,
                                 .ok = false,
                                 .summary = "单文件输入不是安全文本/代码格式",
                                 .output = JsonValue::Object{{"root", pathText(root)}}});
        }
        ++scannedFiles;
        const auto content = util::readFileLimited(root, kSearchReadLimit);
        const auto lines = util::splitLines(content);
        for (std::size_t index = 0U; index < lines.size() && matches.size() < maxMatches; ++index) {
            const auto haystack = caseSensitive ? lines[index] : util::lowerAscii(lines[index]);
            if (haystack.find(needle) == std::string::npos) {
                continue;
            }
            matches.push_back(JsonValue::Object{{"path", root.filename().generic_string()},
                                                {"line_number", static_cast<int>(index + 1U)},
                                                {"line", truncateText(lines[index], 240U)}});
        }
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = true,
            .summary = "已搜索单个项目文件，命中 " + std::to_string(matches.size()) + " 行",
            .output = JsonValue::Object{{"root", pathText(root)},
                                        {"query", query},
                                        {"scanned_files", static_cast<int>(scannedFiles)},
                                        {"matches", JsonValue{matches}}}});
    }

    if (!std::filesystem::is_directory(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不是可搜索目录或普通文件",
                             .output = JsonValue::Object{{"root", pathText(root)}}});
    }

    for (std::filesystem::recursive_directory_iterator iter(root, ec), end;
         iter != end && !ec && scannedFiles < maxFiles && matches.size() < maxMatches;
         iter.increment(ec)) {
        if (iter->is_directory(ec) && shouldSkipDirectory(iter->path())) {
            iter.disable_recursion_pending();
            continue;
        }
        if (!iter->is_regular_file(ec) || !isReadableTextLike(iter->path())) {
            continue;
        }
        const auto relative = std::filesystem::relative(iter->path(), root, ec);
        if (ec) {
            continue;
        }
        ++scannedFiles;
        const auto content = util::readFileLimited(iter->path(), kSearchReadLimit);
        const auto lines = util::splitLines(content);
        for (std::size_t index = 0U; index < lines.size() && matches.size() < maxMatches; ++index) {
            const auto haystack = caseSensitive ? lines[index] : util::lowerAscii(lines[index]);
            if (haystack.find(needle) == std::string::npos) {
                continue;
            }
            matches.push_back(JsonValue::Object{{"path", relative.generic_string()},
                                                {"line_number", static_cast<int>(index + 1U)},
                                                {"line", truncateText(lines[index], 240U)}});
        }
    }

    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已搜索项目文本，扫描 " + std::to_string(scannedFiles) + " 个文件，命中 " +
                   std::to_string(matches.size()) + " 行",
        .output = JsonValue::Object{{"root", pathText(root)},
                                    {"query", query},
                                    {"scanned_files", static_cast<int>(scannedFiles)},
                                    {"matches", JsonValue{matches}}}});
}

[[nodiscard]] Result<AgentObservation> runDraftMarkdownRevision(const AgentRunRequest& request,
                                                                const AgentToolCall& callItem) {
    auto path = resolveProjectPath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = path.error(),
                                                                  .output = JsonValue::Object{}});
    }
    if (extensionLower(path.value()) != ".md") {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "只允许生成 Markdown 修订稿",
                             .output = JsonValue::Object{}});
    }
    const auto original = util::readFileLimited(path.value(), 256000U);
    const auto replacement = callItem.input.at("replacement_markdown").asString();
    const auto revised = replacement.empty() ? normalizeMarkdown(original) : replacement;
    const auto workspace = writableRoot(request);
    const auto relative = callItem.input.at("path").asString();
    const auto outputPath = workspace / "markdown-revisions" / relative;
    auto written = util::writeTextFile(outputPath, revised);
    if (!written.ok()) {
        return Result<AgentObservation>::failure(written.error());
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已生成 Markdown 工作区修订稿: " + pathText(outputPath),
        .output = JsonValue::Object{{"workspace_path", pathText(outputPath)},
                                    {"preview", truncateText(revised, kPreviewLimit)}}});
}

[[nodiscard]] Result<AgentObservation> runWriteWorkspaceFile(const AgentRunRequest& request,
                                                             const AgentToolCall& callItem) {
    auto path = resolveWorkspacePath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(AgentObservation{.callId = callItem.id,
                                                                  .toolName = callItem.name,
                                                                  .ok = false,
                                                                  .summary = path.error(),
                                                                  .output = JsonValue::Object{}});
    }

    const auto content = callItem.input.at("content").asString();
    auto written = util::writeTextFile(path.value(), content);
    if (!written.ok()) {
        return Result<AgentObservation>::failure(written.error());
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已写入工作区文件: " + pathText(path.value()),
        .output = JsonValue::Object{{"workspace_path", pathText(path.value())},
                                    {"format", extensionLower(path.value()).empty()
                                                   ? "text"
                                                   : extensionLower(path.value()).substr(1)},
                                    {"preview", truncateText(content, kPreviewLimit)}}});
}

using ToolRunner = Result<AgentObservation> (*)(const AgentRunRequest&, const AgentToolCall&);

[[nodiscard]] const std::vector<std::pair<std::string, ToolRunner>>& toolRunners() {
    static const std::vector<std::pair<std::string, ToolRunner>> runners = {
        {"summarize_audit_session", runSummarizeAuditSession},
        {"list_project_files", runListProjectFiles},
        {"inspect_project_file", runInspectProjectFile},
        {"read_text_file", runReadTextFile},
        {"inspect_archive", runInspectArchive},
        {"search_project_text", runSearchProjectText},
        {"draft_markdown_revision", runDraftMarkdownRevision},
        {"write_workspace_file", runWriteWorkspaceFile}};
    return runners;
}

[[nodiscard]] ToolRunner findRunner(const std::string& name) {
    const auto& runners = toolRunners();
    const auto iter = std::find_if(runners.begin(), runners.end(),
                                   [&](const auto& item) { return item.first == name; });
    return iter == runners.end() ? nullptr : iter->second;
}

} // namespace

std::string toString(AgentEventKind kind) {
    switch (kind) {
    case AgentEventKind::Plan:
        return "plan";
    case AgentEventKind::Tool:
        return "tool";
    case AgentEventKind::Assistant:
        return "assistant";
    case AgentEventKind::System:
        return "system";
    }
    return "system";
}

std::string toString(AgentDecisionKind kind) {
    switch (kind) {
    case AgentDecisionKind::ToolCall:
        return "tool_call";
    case AgentDecisionKind::FinalAnswer:
        return "final_answer";
    }
    return "tool_call";
}

std::string agentCommandHelpText() {
    return "可用命令：/audit 运行缺点评审；/agent <任务> 或 /task <任务> 提交智能体任务；"
           "/status 查看会话状态；/compact 压缩当前审计上下文；/clear 开始新会话。"
           "权限模式和高风险边界在设置中管理；普通输入会作为常规问答或项目评审任务处理。";
}

JsonValue agentRunTraceJson(const AgentRunResult& result) {
    return traceJson(result.plan, result.observations, result.events, result.finalAnswer);
}

void setAgentFinalAnswer(AgentRunResult& result, std::string finalAnswer, std::string context) {
    result.finalAnswer = std::move(finalAnswer);
    for (auto& event : result.events) {
        if (event.kind == AgentEventKind::Assistant) {
            event.text = result.finalAnswer;
            event.context = context;
            event.payload = JsonValue::Object{};
            result.trace = agentRunTraceJson(result);
            return;
        }
    }
    result.events.push_back(AgentEvent{.kind = AgentEventKind::Assistant,
                                       .role = "智能体",
                                       .text = result.finalAnswer,
                                       .context = std::move(context),
                                       .payload = JsonValue::Object{}});
    result.trace = agentRunTraceJson(result);
}

Result<AgentRunResult> AgentRuntime::runLocal(const AgentRunRequest& request) const {
    AgentPlan plan;
    plan.summary = "本地受控探索：未授权 LLM 时只收集可复核上下文，不冒充模型做语义决策。"
                   "启用 Brain 后，大模型会基于这些观察继续选择受控工具。";

    std::vector<AgentObservation> observations;
    if (request.auditResult != nullptr) {
        auto summarize = call("local_1", "summarize_audit_session", "把审计结果压缩为可对话上下文");
        plan.calls.push_back(summarize);
        auto observed = runTool(request, summarize);
        if (!observed.ok()) {
            return Result<AgentRunResult>::failure(observed.error());
        }
        observations.push_back(observed.value());
    }

    if (!request.projectRoot.empty()) {
        auto list = call("local_2", "list_project_files", "枚举项目文件，建立工具上下文",
                         JsonValue::Object{{"max_files", 120}});
        plan.calls.push_back(list);
        auto observed = runTool(request, list);
        if (!observed.ok()) {
            return Result<AgentRunResult>::failure(observed.error());
        }
        observations.push_back(observed.value());

        const auto readablePath = firstReadablePath(observed.value());
        if (!readablePath.empty()) {
            auto read = call("local_3", "read_text_file", "读取一个代表性文本/代码文件",
                             JsonValue::Object{{"path", readablePath}, {"max_bytes", 16000}});
            plan.calls.push_back(read);
            auto readObserved = runTool(request, read);
            if (!readObserved.ok()) {
                return Result<AgentRunResult>::failure(readObserved.error());
            }
            observations.push_back(readObserved.value());
        }
    }

    const auto finalAnswer = finalAnswerFromObservations(request, observations);
    auto events = buildEvents(plan, "本地智能体", observations, finalAnswer);
    auto trace = traceJson(plan, observations, events, finalAnswer);
    return Result<AgentRunResult>::success(AgentRunResult{.plan = std::move(plan),
                                                          .observations = std::move(observations),
                                                          .events = std::move(events),
                                                          .finalAnswer = finalAnswer,
                                                          .trace = std::move(trace),
                                                          .auditResult = std::nullopt});
}

Result<AgentToolExecution>
AgentRuntime::runToolExecution(const AgentRunRequest& request,
                               const AgentToolCall& callItem,
                               AgentObservationObserver observe) const {
    auto specs = ToolRegistry{}.interactiveToolSpecs();
    const auto* toolSpec = findSpec(callItem.name, specs);
    if (toolSpec == nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "未注册或当前不允许 Brain 直接驱动的工具: " + callItem.name,
                .output = JsonValue::Object{}}},
            .auditResult = std::nullopt});
    }

    if (request.requireAudit && request.auditResult == nullptr &&
        callItem.name != "run_project_audit") {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "首次项目审查必须先调用 run_project_audit 建立隔离副本并取得规则结果",
                .output = JsonValue::Object{{"required_tool", "run_project_audit"}}}},
            .auditResult = std::nullopt});
    }

    PermissionGate gate;
    if (request.allowReadExternal) {
        gate.allow(ToolPermission::ReadExternalFiles);
    }
    if (request.allowModifyOriginal) {
        gate.allow(ToolPermission::ModifyOriginalProject);
    }
    if (request.allowExecuteCommand) {
        gate.allow(ToolPermission::ExecuteCommand);
    }
    if (request.allowNetwork) {
        gate.allow(ToolPermission::NetworkAccess);
    } else {
        gate.deny(ToolPermission::NetworkAccess);
    }
    if (request.allowLlm) {
        gate.allow(ToolPermission::LLMAccess);
    } else {
        gate.deny(ToolPermission::LLMAccess);
    }
    if (!gate.isAllowed(toolSpec->permission)) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = permissionDeniedText(toolSpec->permission),
                .output = JsonValue::Object{{"permission", toString(toolSpec->permission)}}}},
            .auditResult = std::nullopt});
    }

    if (callItem.name == "run_project_audit") {
        return runProjectAuditExecution(request, callItem, observe);
    }

    const auto runner = findRunner(callItem.name);
    if (runner == nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{.callId = callItem.id,
                                              .toolName = callItem.name,
                                              .ok = false,
                                              .summary = "工具尚未接入 AgentRuntime: " + callItem.name,
                                              .output = JsonValue::Object{}}},
            .auditResult = std::nullopt});
    }
    auto observed = runner(request, callItem);
    if (!observed.ok()) {
        return Result<AgentToolExecution>::failure(observed.error());
    }
    if (observe) {
        observe(observed.value());
    }
    return Result<AgentToolExecution>::success(
        AgentToolExecution{.observations = {std::move(observed.value())},
                           .auditResult = std::nullopt});
}

Result<AgentObservation> AgentRuntime::runTool(const AgentRunRequest& request,
                                               const AgentToolCall& callItem) const {
    auto execution = runToolExecution(request, callItem);
    if (!execution.ok()) {
        return Result<AgentObservation>::failure(execution.error());
    }
    if (execution.value().observations.empty()) {
        return Result<AgentObservation>::failure("工具执行没有返回观察结果: " + callItem.name);
    }
    return Result<AgentObservation>::success(
        std::move(execution.value().observations.back()));
}

} // namespace cc
