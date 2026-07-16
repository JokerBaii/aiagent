/**
 * @file AgentRuntime.cpp
 * @brief 受控智能体运行时实现。
 */

#include "cc/agent/AgentRuntime.hpp"

#include "cc/agent/AgentFilePolicy.hpp"
#include "cc/agent/AgentPermissionPolicy.hpp"
#include "cc/agent/AgentTraceSerializer.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/agent/ToolRegistry.hpp"
#include "cc/agent/WorkspaceEditor.hpp"
#include "cc/audit/DiffVerifier.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/loader/LibArchiveReader.hpp"
#include "cc/loader/PathGuard.hpp"
#include "cc/loader/ZipArchiveReader.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/text/OpenXmlTextExtractor.hpp"
#include "cc/text/PdfTextExtractor.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"
#include <limits>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cc {
namespace {

using agent_file_policy::hasSensitivePathComponent;
using agent_file_policy::isReadableTextLike;
using agent_file_policy::isSensitiveFile;
using agent_file_policy::sanitizeUtf8;
using agent_file_policy::textContainsSecretMarker;
using agent_file_policy::truncateText;

[[nodiscard]] bool requestCancelled(const AgentRunRequest& request) {
    if (!request.isCancelled) {
        return false;
    }
    try {
        return request.isCancelled();
    } catch (...) {
        return true;
    }
}

[[nodiscard]] bool enforceSensitiveFilePolicy(const AgentRunRequest& request) {
    return request.permissionMode != "full";
}

constexpr std::size_t kDefaultReadLimit = 12000U;
constexpr std::size_t kMaximumReadLimit = 256U * 1024U;
constexpr std::size_t kPreviewLimit = 1200U;
constexpr std::size_t kSearchReadLimit = 256000U;
constexpr std::size_t kMaximumListFiles = 1000U;
constexpr std::size_t kMaximumArchiveEntries = 1000U;
constexpr std::size_t kMaximumSearchFiles = 1000U;
constexpr std::size_t kMaximumSearchMatches = 500U;
constexpr std::size_t kMaximumTraversalEntries = 100000U;

[[nodiscard]] std::string pathText(const std::filesystem::path& path) {
    return path.generic_string();
}

[[nodiscard]] std::string extensionLower(const std::filesystem::path& path) {
    return util::lowerAscii(path.extension().generic_string());
}

[[nodiscard]] std::string projectRootLabel() {
    return "project://";
}

[[nodiscard]] std::string workspacePathLabel(const std::filesystem::path& relative) {
    return "workspace://" + relative.generic_string();
}

[[nodiscard]] std::size_t boundedInteger(const JsonValue& input, const std::string& key,
                                         std::size_t fallback, std::size_t maximum) {
    const auto& value = input.at(key);
    if (!value.isNumber()) {
        return fallback;
    }
    const auto number = value.asNumber();
    if (!std::isfinite(number) || number < 1.0) {
        return fallback;
    }
    const auto bounded = std::min(number, static_cast<double>(maximum));
    return static_cast<std::size_t>(bounded);
}

[[nodiscard]] AgentObservation rejectedObservation(const AgentToolCall& callItem,
                                                   std::string summary) {
    return AgentObservation{.callId = callItem.id,
                            .toolName = callItem.name,
                            .ok = false,
                            .summary = std::move(summary),
                            .output = JsonValue::Object{}};
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

[[nodiscard]] Result<std::filesystem::path>
resolveWritableProjectPath(const AgentRunRequest& request, const JsonValue& input) {
    const auto relative = input.at("path").asString();
    if (relative.empty()) {
        return Result<std::filesystem::path>::failure("写入路径不能为空");
    }
    const std::filesystem::path requested{relative};
    if (requested.is_absolute()) {
        if (request.permissionMode != "full") {
            return Result<std::filesystem::path>::failure("绝对路径写入仅完全访问模式可用");
        }
        return PathGuard::normalize(requested);
    }
    if (!PathGuard::isSafeArchiveEntry(relative)) {
        return Result<std::filesystem::path>::failure("拒绝写入不安全项目路径: " + relative);
    }
    const auto root = readableRoot(request);
    std::error_code ec;
    if (std::filesystem::is_regular_file(root, ec)) {
        if (relative != root.filename().generic_string()) {
            return Result<std::filesystem::path>::failure(
                "当前项目输入是单个文件，只能覆盖该文件: " + root.filename().generic_string());
        }
        return PathGuard::normalize(root);
    }
    if (!std::filesystem::is_directory(root, ec)) {
        return Result<std::filesystem::path>::failure("原项目目录不存在或不可写");
    }
    const auto target = root / requested;
    if (!PathGuard::isInsideRoot(root, target)) {
        return Result<std::filesystem::path>::failure("拒绝写入原项目边界外路径: " + relative);
    }
    return PathGuard::normalize(target);
}

struct ShellCommandResult {
    int exitCode{-1};
    bool cancelled{false};
    std::string output;
};

[[nodiscard]] Result<ShellCommandResult> executeBashCommand(const AgentRunRequest& request,
                                                            const std::string& command) {
#if defined(__unix__) || defined(__APPLE__)
    const auto root = readableRoot(request);
    std::error_code filesystemError;
    const auto workingDirectory =
        std::filesystem::is_regular_file(root, filesystemError) ? root.parent_path() : root;
    if (workingDirectory.empty() ||
        !std::filesystem::is_directory(workingDirectory, filesystemError)) {
        return Result<ShellCommandResult>::failure("Shell 工作目录不存在");
    }

    int outputPipe[2] = {-1, -1};
    if (::pipe(outputPipe) != 0) {
        return Result<ShellCommandResult>::failure("无法创建 Shell 输出管道");
    }
    const auto child = ::fork();
    if (child < 0) {
        ::close(outputPipe[0]);
        ::close(outputPipe[1]);
        return Result<ShellCommandResult>::failure("无法启动 Bash 进程");
    }
    if (child == 0) {
        static_cast<void>(::setpgid(0, 0));
        ::close(outputPipe[0]);
        static_cast<void>(::dup2(outputPipe[1], STDOUT_FILENO));
        static_cast<void>(::dup2(outputPipe[1], STDERR_FILENO));
        ::close(outputPipe[1]);
        if (::chdir(workingDirectory.c_str()) != 0) {
            ::_exit(126);
        }
        ::execl("/bin/bash", "bash", "-lc", command.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }

    ::close(outputPipe[1]);
    const auto currentFlags = ::fcntl(outputPipe[0], F_GETFL, 0);
    if (currentFlags >= 0) {
        static_cast<void>(::fcntl(outputPipe[0], F_SETFL, currentFlags | O_NONBLOCK));
    }
    ShellCommandResult result;
    int childStatus = 0;
    bool childExited = false;
    const auto drainOutput = [&]() {
        char buffer[4096];
        while (true) {
            const auto count = ::read(outputPipe[0], buffer, sizeof(buffer));
            if (count > 0) {
                const auto bytes = static_cast<std::size_t>(count);
                result.output.append(buffer, bytes);
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
    };

    while (!childExited) {
        drainOutput();
        const auto waited = ::waitpid(child, &childStatus, WNOHANG);
        if (waited == child) {
            childExited = true;
            break;
        }
        result.cancelled = requestCancelled(request);
        if (result.cancelled) {
            static_cast<void>(::kill(-child, SIGKILL));
            static_cast<void>(::kill(child, SIGKILL));
            while (::waitpid(child, &childStatus, 0) < 0 && errno == EINTR) {
            }
            childExited = true;
            break;
        }
        pollfd descriptor{.fd = outputPipe[0], .events = POLLIN, .revents = 0};
        static_cast<void>(::poll(&descriptor, 1, 50));
    }
    drainOutput();
    ::close(outputPipe[0]);
    if (WIFEXITED(childStatus)) {
        result.exitCode = WEXITSTATUS(childStatus);
    } else if (WIFSIGNALED(childStatus)) {
        result.exitCode = 128 + WTERMSIG(childStatus);
    }
    result.output = sanitizeUtf8(result.output);
    return Result<ShellCommandResult>::success(std::move(result));
#else
    static_cast<void>(request);
    static_cast<void>(command);
    return Result<ShellCommandResult>::failure("当前平台未提供 /bin/bash，无法执行 Shell 命令");
#endif
}

[[nodiscard]] AgentToolCall call(std::string id, std::string name, std::string reason,
                                 JsonValue input = JsonValue::Object{}) {
    return AgentToolCall{.id = std::move(id),
                         .name = std::move(name),
                         .reason = std::move(reason),
                         .input = std::move(input),
                         .rawArguments = {},
                         .assistantContent = {},
                         .reasoningContent = {}};
}

[[nodiscard]] bool hasRiskFlag(const ProjectAsset& asset, std::string_view flag) {
    return std::find(asset.riskFlags.begin(), asset.riskFlags.end(), flag) != asset.riskFlags.end();
}

[[nodiscard]] bool contentDeferred(const ProjectAsset& asset) {
    return hasRiskFlag(asset, "CONTENT_DEFERRED");
}

[[nodiscard]] bool assetTextReadable(const ProjectAsset& asset) {
    if (contentDeferred(asset) || !asset.auditable) {
        return false;
    }
    return asset.mime.starts_with("text/") || !asset.language.empty();
}

[[nodiscard]] JsonValue assetJson(const ProjectAsset& asset) {
    JsonValue::Array riskFlags;
    riskFlags.reserve(asset.riskFlags.size());
    for (const auto& flag : asset.riskFlags) {
        riskFlags.emplace_back(flag);
    }
    return JsonValue::Object{{"path", asset.relativePath.generic_string()},
                             {"name", asset.fileName},
                             {"extension", asset.extension},
                             {"size_bytes", static_cast<double>(asset.sizeBytes)},
                             {"format", asset.format},
                             {"mime", asset.mime},
                             {"language", asset.language},
                             {"role", toString(asset.role)},
                             {"text_readable", assetTextReadable(asset)},
                             {"auditable", asset.auditable},
                             {"content_deferred", contentDeferred(asset)},
                             {"on_demand_text_candidate", false},
                             {"risk_flags", JsonValue{std::move(riskFlags)}}};
}

[[nodiscard]] ProjectAsset detectProjectAsset(const AgentRunRequest& request,
                                              const std::filesystem::path& file) {
    const auto root = readableRoot(request);
    std::error_code ec;
    const auto base = std::filesystem::is_regular_file(root, ec) ? root.parent_path() : root;
    return FormatDetector{}.detect(base.empty() ? root : base, file);
}

[[nodiscard]] std::filesystem::path requestedRelativePath(const AgentRunRequest& request,
                                                          const JsonValue& input) {
    const auto raw = input.at("path").asString();
    std::error_code ec;
    const auto root = readableRoot(request);
    if (std::filesystem::is_regular_file(root, ec) &&
        (raw.empty() || raw == "." || raw == "./" || raw == root.filename().generic_string())) {
        return root.filename();
    }
    return std::filesystem::path{raw}.lexically_normal();
}

[[nodiscard]] const ProjectAsset* findAuditAsset(const AgentRunRequest& request,
                                                 const JsonValue& input) {
    if (request.auditResult == nullptr) {
        return nullptr;
    }
    const auto relative = requestedRelativePath(request, input);
    const auto found =
        std::find_if(request.auditResult->inventory.assets.begin(),
                     request.auditResult->inventory.assets.end(), [&](const ProjectAsset& asset) {
                         return asset.relativePath.lexically_normal() == relative;
                     });
    return found == request.auditResult->inventory.assets.end() ? nullptr : &(*found);
}

[[nodiscard]] bool manifestAssetSensitive(const ProjectAsset& asset) {
    return asset.sensitive || hasSensitivePathComponent(asset.relativePath);
}

[[nodiscard]] bool hasSymlinkComponent(const std::filesystem::path& root,
                                       const std::filesystem::path& relative) {
    std::error_code ec;
    auto current = root;
    for (const auto& component : relative) {
        current /= component;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(current, ec))) {
            return true;
        }
        if (ec) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] Result<std::filesystem::path>
resolveDeferredOriginalFile(const AgentRunRequest& request, const ProjectAsset& asset) {
    if (request.auditResult == nullptr || !contentDeferred(asset)) {
        return Result<std::filesystem::path>::failure("该文件不属于延迟内容清单");
    }
    const auto& context = request.auditResult->context;
    if (context.archiveInput) {
        return Result<std::filesystem::path>::failure(
            "该文件位于归档内部；未安全解压的内容不能绕过归档边界直接读取");
    }
    if (ArchiveExtractor::isArchivePath(context.originalRoot)) {
        return Result<std::filesystem::path>::failure(
            "归档容器不能作为普通延迟文件绕过归档安全策略直接读取");
    }
    std::error_code ec;
    if (std::filesystem::is_regular_file(context.originalRoot, ec)) {
        ec.clear();
        if (std::filesystem::is_symlink(
                std::filesystem::symlink_status(context.originalRoot, ec)) ||
            ec) {
            return Result<std::filesystem::path>::failure(
                "用户所选单文件是符号链接，不能作为延迟内容读取");
        }
        if (asset.relativePath.lexically_normal() != context.originalRoot.filename()) {
            return Result<std::filesystem::path>::failure("延迟清单项与用户所选单文件不一致");
        }
        auto normalized = PathGuard::normalize(context.originalRoot);
        if (!normalized.ok()) {
            return Result<std::filesystem::path>::failure("无法安全定位用户所选的延迟单文件");
        }
        return normalized;
    }
    ec.clear();
    if (!std::filesystem::is_directory(context.originalRoot, ec)) {
        return Result<std::filesystem::path>::failure("延迟文件没有可安全读取的原始来源");
    }
    const auto relative = asset.relativePath.lexically_normal();
    if (!PathGuard::isSafeArchiveEntry(relative.generic_string())) {
        return Result<std::filesystem::path>::failure("延迟文件路径未通过安全校验");
    }
    const auto target = context.originalRoot / relative;
    if (!PathGuard::isInsideRoot(context.originalRoot, target) ||
        hasSymlinkComponent(context.originalRoot, relative)) {
        return Result<std::filesystem::path>::failure("延迟文件路径包含符号链接或越过用户所选目录");
    }
    if (!std::filesystem::is_regular_file(target, ec)) {
        return Result<std::filesystem::path>::failure("延迟文件的原始来源已不存在或不是普通文件");
    }
    auto normalized = PathGuard::normalize(target);
    if (!normalized.ok() || !PathGuard::isInsideRoot(context.originalRoot, normalized.value())) {
        return Result<std::filesystem::path>::failure("无法安全定位延迟文件的原始来源");
    }
    return normalized;
}

[[nodiscard]] bool onDemandTextCandidate(const AgentRunRequest& request,
                                         const ProjectAsset& asset) {
    if (request.auditResult == nullptr || !contentDeferred(asset) ||
        request.auditResult->context.archiveInput ||
        ArchiveExtractor::isArchivePath(request.auditResult->context.originalRoot)) {
        return false;
    }
    const auto source = resolveDeferredOriginalFile(request, asset);
    return source.ok();
}

[[nodiscard]] const TextDocument* extractedProjectDocument(const AgentRunRequest& request,
                                                           const std::filesystem::path& relative) {
    if (request.auditResult == nullptr) {
        return nullptr;
    }
    const auto document =
        std::find_if(request.auditResult->corpus.begin(), request.auditResult->corpus.end(),
                     [&](const TextDocument& item) { return item.sourceFile == relative; });
    return document == request.auditResult->corpus.end() ? nullptr : &(*document);
}

[[nodiscard]] JsonValue requestAssetJson(const AgentRunRequest& request,
                                         const ProjectAsset& asset) {
    auto value = assetJson(asset);
    value.asObject().insert_or_assign("on_demand_text_candidate",
                                      onDemandTextCandidate(request, asset));
    value.asObject().insert_or_assign("extracted_document_available",
                                      extractedProjectDocument(request, asset.relativePath) !=
                                          nullptr);
    return value;
}

[[nodiscard]] std::filesystem::path originalDetectionRoot(const ProjectContext& context) {
    std::error_code ec;
    return std::filesystem::is_regular_file(context.originalRoot, ec)
               ? context.originalRoot.parent_path()
               : context.originalRoot;
}

[[nodiscard]] bool confirmedTextAsset(const ProjectAsset& asset) {
    return asset.auditable && (asset.mime.starts_with("text/") || !asset.language.empty());
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
    output << "本轮已由本地运行时完成可复核的上下文收集；如果这是模型调用失败后的自动"
              "恢复，以下工具结果仍然有效。";
    const bool enumerateOriginal =
        request.permissionMode == "bypass" || request.permissionMode == "full";
    if (request.auditResult != nullptr && !enumerateOriginal) {
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
        calls.push_back(agentToolCallJson(item));
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
                      .payload = agentObservationJson(observation)};
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
    if (requestCancelled(request)) {
        return Result<AgentToolExecution>::failure("智能体任务已取消");
    }
    if (request.auditResult != nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "当前会话已有审计结果，无需重复运行；请读取摘要或继续核查文件",
                .output = JsonValue::Object{}}},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }
    if (request.projectRoot.empty()) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{.callId = callItem.id,
                                              .toolName = callItem.name,
                                              .ok = false,
                                              .summary = "未提供可审计的项目路径",
                                              .output = JsonValue::Object{}}},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }

    StagedAuditPipeline pipeline;
    auto begun = pipeline.begin(request.projectRoot, request.auditOptions);
    if (!begun.ok()) {
        return Result<AgentToolExecution>::failure(begun.error());
    }

    std::vector<AgentObservation> observations;
    observations.reserve(StagedAuditPipeline::stages().size() + 1U);
    while (pipeline.hasNext()) {
        if (requestCancelled(request)) {
            return Result<AgentToolExecution>::failure("智能体任务已取消");
        }
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
        .summary = "已完成确定性规则审计，结果已交回 DeepSeek 继续分析",
        .output = JsonValue::Object{
            {"summary", summary},
            {"session_id", result.value().context.sessionId},
            {"project_root", projectRootLabel()},
            {"workspace_root", "workspace://"},
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
                           .auditResult = std::move(result.value()),
                           .auditDiff = std::nullopt});
}

[[nodiscard]] Result<AgentObservation> runListProjectFiles(const AgentRunRequest& request,
                                                           const AgentToolCall& callItem) {
    const auto maxFiles = boundedInteger(callItem.input, "max_files", 80U, kMaximumListFiles);
    if (request.auditResult != nullptr) {
        JsonValue::Array files;
        std::size_t omittedSensitive = 0U;
        std::size_t visibleCount = 0U;
        std::size_t deferredCount = 0U;
        for (const auto& asset : request.auditResult->inventory.assets) {
            if (manifestAssetSensitive(asset)) {
                ++omittedSensitive;
                continue;
            }
            ++visibleCount;
            if (contentDeferred(asset)) {
                ++deferredCount;
            }
            if (files.size() < maxFiles) {
                files.push_back(requestAssetJson(request, asset));
            }
        }
        const bool truncated = visibleCount > files.size();
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = true,
            .summary = "已从审计清单列出可安全暴露的项目文件 " + std::to_string(files.size()) +
                       " 个" +
                       (deferredCount == 0U ? std::string{}
                                            : "，其中清单保留 " + std::to_string(deferredCount) +
                                                  " 个仅元数据或按需读取的文件") +
                       (omittedSensitive == 0U
                            ? std::string{}
                            : "；" + std::to_string(omittedSensitive) + " 个敏感文件已隐藏"),
            .output = JsonValue::Object{
                {"root", projectRootLabel()},
                {"source", "audit_inventory"},
                {"omitted_sensitive_count", static_cast<double>(omittedSensitive)},
                {"deferred_count", static_cast<double>(deferredCount)},
                {"truncated", truncated},
                {"files", JsonValue{std::move(files)}}}});
    }

    const auto root = readableRoot(request);
    std::error_code ec;
    if (root.empty() || !std::filesystem::exists(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不存在或不可访问",
                             .output = JsonValue::Object{{"root", projectRootLabel()}}});
    }

    JsonValue::Array files;
    std::size_t omittedSensitive = 0U;
    std::size_t visitedEntries = 0U;
    bool traversalTruncated = false;

    if (std::filesystem::is_regular_file(root, ec)) {
        if (enforceSensitiveFilePolicy(request) && isSensitiveFile(root)) {
            return Result<AgentObservation>::success(rejectedObservation(
                callItem, "该文件命中敏感信息策略，未向智能体暴露文件名、元数据或内容"));
        }
        auto asset = detectProjectAsset(request, root);
        asset.relativePath = root.filename();
        files.push_back(assetJson(asset));
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = true,
                             .summary = "当前输入是单个项目文件，已读取文件元数据",
                             .output = JsonValue::Object{{"root", projectRootLabel()},
                                                         {"omitted_sensitive_count", 0},
                                                         {"files", JsonValue{files}}}});
    }

    if (!std::filesystem::is_directory(root, ec)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "项目路径不是可枚举目录或普通文件",
                             .output = JsonValue::Object{{"root", projectRootLabel()}}});
    }

    for (std::filesystem::recursive_directory_iterator iter(root, ec), end;
         iter != end && !ec && visitedEntries < kMaximumTraversalEntries; iter.increment(ec)) {
        ++visitedEntries;
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
        if (!PathGuard::isInsideRoot(root, iter->path())) {
            continue;
        }
        if (enforceSensitiveFilePolicy(request) && isSensitiveFile(iter->path())) {
            ++omittedSensitive;
            continue;
        }
        auto asset = detectProjectAsset(request, iter->path());
        asset.relativePath = relative;
        files.push_back(assetJson(asset));
        if (files.size() >= maxFiles) {
            break;
        }
    }
    traversalTruncated = visitedEntries >= kMaximumTraversalEntries;
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已枚举可安全暴露的项目文件 " + std::to_string(files.size()) + " 个" +
                   (omittedSensitive == 0U
                        ? std::string{}
                        : "，另有 " + std::to_string(omittedSensitive) + " 个敏感文件已隐藏"),
        .output =
            JsonValue::Object{{"root", projectRootLabel()},
                              {"omitted_sensitive_count", static_cast<double>(omittedSensitive)},
                              {"truncated", traversalTruncated || files.size() >= maxFiles},
                              {"files", JsonValue{files}}}});
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
    const auto* manifestAsset = findAuditAsset(request, callItem.input);
    if (enforceSensitiveFilePolicy(request) && manifestAsset != nullptr &&
        manifestAssetSensitive(*manifestAsset)) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "该文件命中敏感信息策略，未向智能体暴露文件名、元数据或内容"));
    }
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(path.value())) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "该文件命中敏感信息策略，未向智能体暴露文件名、元数据或内容"));
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path.value(), ec)) {
        if (manifestAsset != nullptr && contentDeferred(*manifestAsset)) {
            bool canReadText = false;
            auto source = resolveDeferredOriginalFile(request, *manifestAsset);
            if (source.ok()) {
                if (enforceSensitiveFilePolicy(request) && isSensitiveFile(source.value())) {
                    return Result<AgentObservation>::success(rejectedObservation(
                        callItem, "该文件命中敏感信息策略，未向智能体暴露文件名、元数据或内容"));
                }
                const auto detected = FormatDetector{}.detect(
                    originalDetectionRoot(request.auditResult->context), source.value());
                canReadText = confirmedTextAsset(detected);
            }
            return Result<AgentObservation>::success(AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = true,
                .summary = "已从审计清单检查文件元数据；内容未载入项目工作副本",
                .output = JsonValue::Object{
                    {"path", manifestAsset->relativePath.generic_string()},
                    {"asset", requestAssetJson(request, *manifestAsset)},
                    {"content_deferred", true},
                    {"on_demand_text_candidate", onDemandTextCandidate(request, *manifestAsset)},
                    {"can_read_text", canReadText},
                    {"can_inspect_archive", false},
                    {"suggested_tool", canReadText ? "read_text_file" : "list_project_files"}}});
        }
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "目标不是项目内普通文件",
                             .output = JsonValue::Object{}});
    }
    const auto asset = detectProjectAsset(request, path.value());
    const bool canReadText = confirmedTextAsset(asset);
    const bool canReadExtractedDocument =
        asset.auditable &&
        (isOfficeExtension(asset.extension) || asset.mime == "application/pdf" ||
         asset.format == "docx" || asset.format == "pptx" || asset.format == "xlsx");
    const bool canInspectArchive = ArchiveExtractor::isArchivePath(path.value());
    const auto suggested = canReadText                ? "read_text_file"
                           : canReadExtractedDocument ? "read_extracted_document"
                           : canInspectArchive        ? "inspect_archive"
                                                      : "list_project_files";
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已检查文件格式: " + pathText(asset.relativePath),
        .output = JsonValue::Object{{"path", pathText(asset.relativePath)},
                                    {"asset", assetJson(asset)},
                                    {"content_deferred", false},
                                    {"can_read_text", canReadText},
                                    {"can_read_extracted_document", canReadExtractedDocument},
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
    const auto* manifestAsset = findAuditAsset(request, callItem.input);
    if (enforceSensitiveFilePolicy(request) && manifestAsset != nullptr &&
        manifestAssetSensitive(*manifestAsset)) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "该文件命中敏感信息策略，已拒绝读取且不会把内容发送给模型"));
    }
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(path.value())) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "该文件命中敏感信息策略，已拒绝读取且不会把内容发送给模型"));
    }
    const auto limit =
        boundedInteger(callItem.input, "max_bytes", kDefaultReadLimit, kMaximumReadLimit);
    std::error_code ec;
    if (std::filesystem::is_regular_file(path.value(), ec)) {
        const auto asset = detectProjectAsset(request, path.value());
        if (!confirmedTextAsset(asset)) {
            return Result<AgentObservation>::success(
                AgentObservation{.callId = callItem.id,
                                 .toolName = callItem.name,
                                 .ok = false,
                                 .summary = "该文件不是可安全读取的文本或代码格式",
                                 .output = JsonValue::Object{{"asset", assetJson(asset)},
                                                             {"content_deferred", false}}});
        }
        const auto content = truncateText(util::readFileLimited(path.value(), limit), limit);
        const auto sizeBytes = std::filesystem::file_size(path.value(), ec);
        const bool truncated = !ec && sizeBytes > limit;
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = true,
            .summary = "已读取文本/代码文件: " + callItem.input.at("path").asString(),
            .output = JsonValue::Object{{"path", callItem.input.at("path").asString()},
                                        {"asset", assetJson(asset)},
                                        {"content_deferred", false},
                                        {"truncated", truncated},
                                        {"content", content}}});
    }

    if (manifestAsset == nullptr || !contentDeferred(*manifestAsset)) {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "目标不是项目内普通文件，审计清单中也没有可按需读取的内容",
                             .output = JsonValue::Object{}});
    }

    auto source = resolveDeferredOriginalFile(request, *manifestAsset);
    if (!source.ok()) {
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = false,
            .summary = source.error(),
            .output = JsonValue::Object{{"asset", requestAssetJson(request, *manifestAsset)},
                                        {"content_deferred", true}}});
    }
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(source.value())) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "延迟文件的原始内容命中敏感信息策略，已拒绝读取且不会发送给模型"));
    }
    const auto detected = FormatDetector{}.detect(
        originalDetectionRoot(request.auditResult->context), source.value());
    if (!confirmedTextAsset(detected)) {
        return Result<AgentObservation>::success(AgentObservation{
            .callId = callItem.id,
            .toolName = callItem.name,
            .ok = false,
            .summary = "延迟文件经有界内容检测后不是可安全读取的文本或代码格式",
            .output = JsonValue::Object{{"asset", requestAssetJson(request, *manifestAsset)},
                                        {"content_deferred", true}}});
    }
    const auto content = truncateText(util::readFileLimited(source.value(), limit), limit);
    const auto sizeBytes = std::filesystem::file_size(source.value(), ec);
    const bool truncated = !ec && sizeBytes > limit;
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已从用户选择的原始来源按需限量读取延迟文本文件: " +
                   manifestAsset->relativePath.generic_string(),
        .output = JsonValue::Object{{"path", manifestAsset->relativePath.generic_string()},
                                    {"asset", requestAssetJson(request, *manifestAsset)},
                                    {"content_deferred", true},
                                    {"on_demand_original_read", true},
                                    {"truncated", truncated},
                                    {"content", content}}});
}

[[nodiscard]] Result<AgentObservation> runReadExtractedDocument(const AgentRunRequest& request,
                                                                const AgentToolCall& callItem) {
    auto path = resolveProjectPath(request, callItem.input);
    if (!path.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, path.error()));
    }
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(path.value())) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "文档路径未通过项目边界或敏感信息策略"));
    }
    auto asset = detectProjectAsset(request, path.value());
    if (!asset.auditable) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "文件内容与文档格式不匹配，无法直接抽取"));
    }
    if (!isOfficeExtension(asset.extension) &&
        (asset.format == "docx" || asset.format == "pptx" || asset.format == "xlsx")) {
        asset.extension = "." + asset.format;
    }
    Result<TextDocument> extracted = Result<TextDocument>::failure("不支持直接抽取该文档格式");
    if (isOfficeExtension(asset.extension)) {
        extracted = OpenXmlTextExtractor{}.extract(asset);
    } else if (asset.mime == "application/pdf" || asset.extension == ".pdf") {
        extracted = PdfTextExtractor{}.extract(asset);
    }
    if (!extracted.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, extracted.error()));
    }
    const auto& document = extracted.value();
    const auto limit = boundedInteger(callItem.input, "max_bytes", 24000U, kMaximumReadLimit);
    const bool needsReview =
        document.status.starts_with("NEED_REVIEW") || document.status == "EMPTY_OR_UNREADABLE";
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = needsReview ? "已读取真实项目文件，但其中部分内容未能完整提取，需要人工复核"
                               : "已直接读取原项目中的办公/PDF 文件内容",
        .output = JsonValue::Object{{"path", asset.relativePath.generic_string()},
                                    {"asset", assetJson(asset)},
                                    {"source", "original_project_file"},
                                    {"extracted_from_project_file", true},
                                    {"direct_original_read", true},
                                    {"status", document.status},
                                    {"content", truncateText(document.text, limit)}}});
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
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(path.value())) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "该压缩包命中敏感信息策略，已拒绝枚举其内容"));
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
        boundedInteger(callItem.input, "max_entries", 120U, kMaximumArchiveEntries);
    JsonValue::Array entries;
    bool safeToExtract = true;
    bool truncated = false;
    std::size_t omittedSensitive = 0U;

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
            if (enforceSensitiveFilePolicy(request) &&
                hasSensitivePathComponent(entry.relativePath)) {
                ++omittedSensitive;
                safeToExtract = false;
                continue;
            }
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
            .output = JsonValue::Object{
                {"path", pathText(asset.relativePath)},
                {"asset", assetJson(asset)},
                {"supported", true},
                {"safe_to_extract", safeToExtract},
                {"omitted_sensitive_count", static_cast<double>(omittedSensitive)},
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
        if (enforceSensitiveFilePolicy(request) && hasSensitivePathComponent(entry.relativePath)) {
            ++omittedSensitive;
            safeToExtract = false;
            continue;
        }
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
        .output =
            JsonValue::Object{{"path", pathText(asset.relativePath)},
                              {"asset", assetJson(asset)},
                              {"supported", true},
                              {"safe_to_extract", safeToExtract},
                              {"omitted_sensitive_count", static_cast<double>(omittedSensitive)},
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

    const auto maxFiles = boundedInteger(callItem.input, "max_files", 80U, kMaximumSearchFiles);
    const auto maxMatches =
        boundedInteger(callItem.input, "max_matches", 40U, kMaximumSearchMatches);
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
                             .output = JsonValue::Object{{"root", projectRootLabel()}}});
    }

    if (std::filesystem::is_regular_file(root, ec)) {
        if (enforceSensitiveFilePolicy(request) && isSensitiveFile(root)) {
            return Result<AgentObservation>::success(rejectedObservation(
                callItem, "该文件命中敏感信息策略，已拒绝搜索且不会把内容发送给模型"));
        }
        if (!isReadableTextLike(root)) {
            return Result<AgentObservation>::success(
                AgentObservation{.callId = callItem.id,
                                 .toolName = callItem.name,
                                 .ok = false,
                                 .summary = "单文件输入不是安全文本/代码格式",
                                 .output = JsonValue::Object{{"root", projectRootLabel()}}});
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
            .output = JsonValue::Object{{"root", projectRootLabel()},
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
                             .output = JsonValue::Object{{"root", projectRootLabel()}}});
    }

    std::size_t visitedEntries = 0U;
    std::size_t omittedSensitive = 0U;
    for (std::filesystem::recursive_directory_iterator iter(root, ec), end;
         iter != end && !ec && scannedFiles < maxFiles && matches.size() < maxMatches &&
         visitedEntries < kMaximumTraversalEntries;
         iter.increment(ec)) {
        ++visitedEntries;
        if (iter->is_directory(ec) && shouldSkipDirectory(iter->path())) {
            iter.disable_recursion_pending();
            continue;
        }
        if (!iter->is_regular_file(ec)) {
            continue;
        }
        if (!PathGuard::isInsideRoot(root, iter->path())) {
            continue;
        }
        if (enforceSensitiveFilePolicy(request) && isSensitiveFile(iter->path())) {
            ++omittedSensitive;
            continue;
        }
        if (!isReadableTextLike(iter->path())) {
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
        .output = JsonValue::Object{
            {"root", projectRootLabel()},
            {"query", query},
            {"scanned_files", static_cast<int>(scannedFiles)},
            {"omitted_sensitive_count", static_cast<double>(omittedSensitive)},
            {"truncated", visitedEntries >= kMaximumTraversalEntries || scannedFiles >= maxFiles ||
                              matches.size() >= maxMatches},
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
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(path.value())) {
        return Result<AgentObservation>::success(rejectedObservation(
            callItem, "该 Markdown 文件命中敏感信息策略，已拒绝读取和生成修订稿"));
    }
    if (extensionLower(path.value()) != ".md") {
        return Result<AgentObservation>::success(
            AgentObservation{.callId = callItem.id,
                             .toolName = callItem.name,
                             .ok = false,
                             .summary = "只允许生成 Markdown 修订稿",
                             .output = JsonValue::Object{}});
    }
    const auto original = sanitizeUtf8(util::readFileLimited(path.value(), 256000U));
    const auto replacement = callItem.input.at("replacement_markdown").asString();
    const auto revised = replacement.empty() ? normalizeMarkdown(original) : replacement;
    if (enforceSensitiveFilePolicy(request) && textContainsSecretMarker(revised)) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "修订内容包含疑似凭据或密钥片段，已拒绝写入工作区"));
    }
    const auto workspace = writableRoot(request);
    const auto relative = callItem.input.at("path").asString();
    const auto outputRelative = std::filesystem::path{"markdown-revisions"} / relative;
    const auto outputPath = workspace / outputRelative;
    if (!PathGuard::isInsideRoot(workspace, outputPath)) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "拒绝写入工作区边界外路径"));
    }
    auto written = util::writeTextFile(outputPath, revised);
    if (!written.ok()) {
        return Result<AgentObservation>::failure(written.error());
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已生成 Markdown 工作区修订稿: " + workspacePathLabel(outputRelative),
        .output = JsonValue::Object{{"workspace_path", workspacePathLabel(outputRelative)},
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
    const auto relative = std::filesystem::path{callItem.input.at("path").asString()};
    if (enforceSensitiveFilePolicy(request) &&
        (hasSensitivePathComponent(relative) || textContainsSecretMarker(content))) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "目标路径或内容包含疑似凭据/密钥片段，已拒绝写入工作区"));
    }
    auto written = util::writeTextFile(path.value(), content);
    if (!written.ok()) {
        return Result<AgentObservation>::failure(written.error());
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已写入工作区文件: " + workspacePathLabel(relative),
        .output = JsonValue::Object{{"workspace_path", workspacePathLabel(relative)},
                                    {"format", extensionLower(path.value()).empty()
                                                   ? "text"
                                                   : extensionLower(path.value()).substr(1)},
                                    {"preview", truncateText(content, kPreviewLimit)}}});
}

[[nodiscard]] Result<AgentObservation> runReadExternalTextFile(const AgentRunRequest& request,
                                                               const AgentToolCall& callItem) {
    static_cast<void>(request); // 权限快照已在调度层校验；外部路径不与项目根拼接。
    const std::filesystem::path requested{callItem.input.at("path").asString()};
    if (!requested.is_absolute()) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "项目外文件读取需要绝对路径"));
    }
    const auto normalized = PathGuard::normalize(requested);
    if (!normalized.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, normalized.error()));
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(normalized.value(), ec)) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "项目外路径不是可读取的普通文件"));
    }
    if (enforceSensitiveFilePolicy(request) && isSensitiveFile(normalized.value())) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "项目外文件命中敏感信息策略，已拒绝读取"));
    }
    if (!isReadableTextLike(normalized.value())) {
        return Result<AgentObservation>::success(
            rejectedObservation(callItem, "项目外文件不是可读取的文本或代码格式"));
    }
    const auto limit = boundedInteger(callItem.input, "max_bytes", 24000U, kMaximumReadLimit);
    const auto size = std::filesystem::file_size(normalized.value(), ec);
    const auto content = truncateText(util::readFileLimited(normalized.value(), limit), limit);
    return Result<AgentObservation>::success(
        AgentObservation{.callId = callItem.id,
                         .toolName = callItem.name,
                         .ok = true,
                         .summary = "已读取用户指定的项目外文本文件",
                         .output = JsonValue::Object{{"path", pathText(normalized.value())},
                                                     {"truncated", !ec && size > limit},
                                                     {"content", content}}});
}

[[nodiscard]] Result<AgentObservation> runWriteProjectFile(const AgentRunRequest& request,
                                                           const AgentToolCall& callItem) {
    const auto relative = std::filesystem::path{callItem.input.at("path").asString()};
    const auto content = callItem.input.at("content").asString();
    const auto target = resolveWritableProjectPath(request, callItem.input);
    if (!target.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, target.error()));
    }
    const auto written = util::writeTextFile(target.value(), content);
    if (!written.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, written.error()));
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已直接写入原项目文件: " + relative.generic_string(),
        .output = JsonValue::Object{{"path", relative.generic_string()},
                                    {"format", extensionLower(target.value()).empty()
                                                   ? "text"
                                                   : extensionLower(target.value()).substr(1)},
                                    {"preview", truncateText(content, kPreviewLimit)}}});
}

[[nodiscard]] Result<AgentObservation> runExecuteShellCommand(const AgentRunRequest& request,
                                                              const AgentToolCall& callItem) {
    const auto executed = executeBashCommand(request, callItem.input.at("command").asString());
    if (!executed.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, executed.error()));
    }
    const auto& result = executed.value();
    const auto ok = result.exitCode == 0 && !result.cancelled;
    std::string summary;
    if (result.cancelled) {
        summary = "Shell/Bash 命令已取消";
    } else {
        summary = "Shell/Bash 命令执行完成，退出码 " + std::to_string(result.exitCode);
    }
    return Result<AgentObservation>::success(
        AgentObservation{.callId = callItem.id,
                         .toolName = callItem.name,
                         .ok = ok,
                         .summary = std::move(summary),
                         .output = JsonValue::Object{{"command", callItem.input.at("command")},
                                                     {"exit_code", result.exitCode},
                                                     {"cancelled", result.cancelled},
                                                     {"output", result.output}}});
}

[[nodiscard]] Result<AgentObservation> runPrepareRepairedWorkspace(const AgentRunRequest& request,
                                                                   const AgentToolCall& callItem) {
    const auto prepared = WorkspaceEditor{}.prepare(request.projectRoot, request.workspaceRoot);
    if (!prepared.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, prepared.error()));
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已建立 repaired project；后续修改不会覆盖原项目",
        .output = JsonValue::Object{{"repaired_root", "workspace://repaired-project/"}}});
}

[[nodiscard]] Result<AgentObservation> runApplyRepairedTextEdit(const AgentRunRequest& request,
                                                                const AgentToolCall& callItem) {
    const auto expectedOccurrences =
        boundedInteger(callItem.input, "expected_occurrences", 1U, 100U);
    const auto edited = WorkspaceEditor{}.applyTextEdit(
        request.projectRoot, request.workspaceRoot, callItem.input.at("path").asString(),
        callItem.input.at("expected_text").asString(),
        callItem.input.at("replacement_text").asString(), expectedOccurrences);
    if (!edited.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, edited.error()));
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已在 repaired project 精确修改 " +
                   edited.value().relativePath.generic_string() + "，并更新统一 diff",
        .output =
            JsonValue::Object{{"path", edited.value().relativePath.generic_string()},
                              {"patch", "workspace://changes.patch"},
                              {"diff", truncateText(edited.value().diff, 12000U)},
                              {"preview", truncateText(edited.value().preview, kPreviewLimit)}}});
}

[[nodiscard]] Result<AgentObservation> runCreateRepairedTextFile(const AgentRunRequest& request,
                                                                 const AgentToolCall& callItem) {
    const auto created = WorkspaceEditor{}.createTextFile(
        request.projectRoot, request.workspaceRoot, callItem.input.at("path").asString(),
        callItem.input.at("content").asString());
    if (!created.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, created.error()));
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已在 repaired project 新建 " + created.value().relativePath.generic_string() +
                   "，并更新统一 diff",
        .output =
            JsonValue::Object{{"path", created.value().relativePath.generic_string()},
                              {"patch", "workspace://changes.patch"},
                              {"diff", truncateText(created.value().diff, 12000U)},
                              {"preview", truncateText(created.value().preview, kPreviewLimit)}}});
}

[[nodiscard]] Result<AgentObservation> runReadRepairedTextFile(const AgentRunRequest& request,
                                                               const AgentToolCall& callItem) {
    const auto limit = boundedInteger(callItem.input, "max_bytes", 24000U, kMaximumReadLimit);
    const auto content = WorkspaceEditor{}.readTextFile(
        request.projectRoot, request.workspaceRoot, callItem.input.at("path").asString(), limit);
    if (!content.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, content.error()));
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "已读回 repaired project 文件，供修改结果复核",
        .output = JsonValue::Object{{"path", callItem.input.at("path").asString()},
                                    {"content", truncateText(content.value(), limit)}}});
}

[[nodiscard]] Result<AgentObservation> runListWorkspaceChanges(const AgentRunRequest& request,
                                                               const AgentToolCall& callItem) {
    const auto changes = WorkspaceEditor{}.changes(request.projectRoot, request.workspaceRoot);
    if (!changes.ok()) {
        return Result<AgentObservation>::success(rejectedObservation(callItem, changes.error()));
    }
    JsonValue::Array items;
    for (const auto& change : changes.value()) {
        items.emplace_back(JsonValue::Object{{"path", change.relativePath.generic_string()},
                                             {"kind", change.kind},
                                             {"diff_preview", truncateText(change.diff, 4000U)}});
    }
    return Result<AgentObservation>::success(AgentObservation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = "repaired project 共有 " + std::to_string(items.size()) + " 个真实变更",
        .output = JsonValue::Object{{"changes", JsonValue{items}},
                                    {"patch", "workspace://changes.patch"}}});
}

[[nodiscard]] Result<AgentToolExecution> runReAuditRepairedProject(const AgentRunRequest& request,
                                                                   const AgentToolCall& callItem) {
    if (request.auditResult == nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {rejectedObservation(callItem, "二次审计需要修复前的确定性审计结果")},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }
    const auto changes = WorkspaceEditor{}.changes(request.projectRoot, request.workspaceRoot);
    if (!changes.ok() || changes.value().empty()) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {rejectedObservation(
                callItem, changes.ok() ? "repaired project 尚无变更" : changes.error())},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }
    const auto* baseline =
        request.baselineAuditResult == nullptr ? request.auditResult : request.baselineAuditResult;
    auto updated = WorkspaceEditor{}.reAudit(request.projectRoot, request.workspaceRoot,
                                             request.auditOptions, &baseline->context);
    if (!updated.ok()) {
        return Result<AgentToolExecution>::failure(updated.error());
    }
    constexpr std::size_t kMaximumPatchBytes = 16U * 1024U * 1024U;
    const auto patchFile = request.workspaceRoot / "changes.patch";
    const auto patch = util::readFileLimited(patchFile, kMaximumPatchBytes + 1U);
    if (patch.empty() || patch.size() > kMaximumPatchBytes) {
        return Result<AgentToolExecution>::failure(
            "真实 changes.patch 为空或超过 16 MiB 上限，已停止二次审计绑定");
    }
    updated.value().repairPlan.diffText = patch;
    updated.value().repairPlan.markdown +=
        "\n\n## repaired-project 实际变更\n\n"
        "真实统一补丁保存在 "
        "`changes.patch`。修改文件均标记为待人工确认草稿，不会作为独立证据抬高评分。\n";
    auto diff = DiffVerifier{}.diffResults(*baseline, updated.value());
    if (!diff.ok()) {
        return Result<AgentToolExecution>::failure(diff.error());
    }
    AgentObservation observation{
        .callId = callItem.id,
        .toolName = callItem.name,
        .ok = true,
        .summary = diff.value().summary,
        .output =
            JsonValue::Object{{"summary", diff.value().summary},
                              {"old_score", diff.value().oldScore},
                              {"new_score", diff.value().newScore},
                              {"audit_diff", auditDiffToJson(diff.value())},
                              {"patch", "workspace://changes.patch"},
                              {"patch_bytes", static_cast<double>(patch.size())},
                              {"changed_file_count", static_cast<double>(changes.value().size())}}};
    return Result<AgentToolExecution>::success(
        AgentToolExecution{.observations = {std::move(observation)},
                           .auditResult = std::move(updated.value()),
                           .auditDiff = std::move(diff.value())});
}

using ToolRunner = Result<AgentObservation> (*)(const AgentRunRequest&, const AgentToolCall&);

[[nodiscard]] const std::vector<std::pair<std::string, ToolRunner>>& toolRunners() {
    static const std::vector<std::pair<std::string, ToolRunner>> runners = {
        {"summarize_audit_session", runSummarizeAuditSession},
        {"list_project_files", runListProjectFiles},
        {"inspect_project_file", runInspectProjectFile},
        {"read_text_file", runReadTextFile},
        {"read_extracted_document", runReadExtractedDocument},
        {"inspect_archive", runInspectArchive},
        {"search_project_text", runSearchProjectText},
        {"prepare_repaired_workspace", runPrepareRepairedWorkspace},
        {"apply_repaired_text_edit", runApplyRepairedTextEdit},
        {"create_repaired_text_file", runCreateRepairedTextFile},
        {"read_repaired_text_file", runReadRepairedTextFile},
        {"list_workspace_changes", runListWorkspaceChanges},
        {"draft_markdown_revision", runDraftMarkdownRevision},
        {"write_workspace_file", runWriteWorkspaceFile},
        {"read_external_text_file", runReadExternalTextFile},
        {"write_project_file", runWriteProjectFile},
        {"execute_shell_command", runExecuteShellCommand}};
    return runners;
}

[[nodiscard]] ToolRunner findRunner(const std::string& name) {
    const auto& runners = toolRunners();
    const auto iter = std::find_if(runners.begin(), runners.end(),
                                   [&](const auto& item) { return item.first == name; });
    return iter == runners.end() ? nullptr : iter->second;
}

} // namespace

std::string agentCommandHelpText() {
    return "可用命令：/audit 运行缺点评审；/agent <任务> 或 /task <任务> 提交智能体任务；"
           "/plan [目标] 生成只读计划；"
           "/optimize [目标] 修改项目并二次审计；/status 查看会话状态；"
           "/compact 压缩当前审计上下文；/clear 开始新会话；/help 显示本帮助。"
           "/full [任务] 使用原项目写入和 Shell/Bash 执行；默认已启用完全访问，"
           "普通输入会作为常规问答或项目任务处理。";
}

Result<AgentRunResult> AgentRuntime::runLocal(const AgentRunRequest& request) const {
    if (requestCancelled(request)) {
        return Result<AgentRunResult>::failure("智能体任务已取消");
    }
    if (request.requireWorkspaceChanges || request.requireReaudit) {
        return Result<AgentRunResult>::failure("项目完善需要有效的智能辅助服务配置来阅读并生成具体"
                                               "修改；本地规则检查只能给出问题和修改清"
                                               "单，不能假装已经完成修改");
    }
    AgentPlan plan;
    plan.summary = "本地受控探索：未配置有效 LLM 时只收集可复核上下文，不冒充模型做语义决策。"
                   "启用 DeepSeek 后，模型会基于这些观察继续选择受控工具。";

    std::vector<AgentObservation> observations;
    std::optional<AuditResult> auditResult;
    if (request.requireAudit && request.auditResult == nullptr && !request.projectRoot.empty()) {
        auto audit =
            call("local_audit", "run_project_audit", "模型不可用时仍执行确定性的项目规则审计");
        plan.calls.push_back(audit);
        auto execution = runToolExecution(request, audit);
        if (!execution.ok()) {
            return Result<AgentRunResult>::failure(execution.error());
        }
        for (auto& observation : execution.value().observations) {
            observations.push_back(std::move(observation));
        }
        auditResult = std::move(execution.value().auditResult);
    }
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
        if (requestCancelled(request)) {
            return Result<AgentRunResult>::failure("智能体任务已取消");
        }
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
    AgentRunResult result{.plan = std::move(plan),
                          .observations = std::move(observations),
                          .events = std::move(events),
                          .finalAnswer = finalAnswer,
                          .trace = JsonValue::Object{},
                          .auditResult = std::move(auditResult),
                          .auditDiff = std::nullopt};
    result.trace = agentRunTraceJson(result);
    return Result<AgentRunResult>::success(std::move(result));
}

Result<AgentToolExecution> AgentRuntime::runToolExecution(const AgentRunRequest& request,
                                                          const AgentToolCall& callItem,
                                                          AgentObservationObserver observe) const {
    if (requestCancelled(request)) {
        return Result<AgentToolExecution>::failure("智能体任务已取消");
    }
    auto specs = ToolRegistry{}.interactiveToolSpecs();
    const auto* toolSpec = findSpec(callItem.name, specs);
    if (toolSpec == nullptr) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "未注册或当前不允许 DeepSeek 直接驱动的工具: " + callItem.name,
                .output = JsonValue::Object{}}},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }

    auto validInput = ToolRegistry{}.validateInteractiveInput(callItem.name, callItem.input);
    if (!validInput.ok()) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = "工具输入校验失败: " + validInput.error(),
                .output = JsonValue::Object{{"reason", "invalid_tool_input"},
                                            {"input_schema", toolSpec->inputSchema},
                                            {"attempted_input", callItem.input}}}},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }

    const auto authorized = AgentPermissionPolicy{}.authorize(request, toolSpec->permission);
    if (!authorized.ok()) {
        return Result<AgentToolExecution>::success(AgentToolExecution{
            .observations = {AgentObservation{
                .callId = callItem.id,
                .toolName = callItem.name,
                .ok = false,
                .summary = authorized.error(),
                .output = JsonValue::Object{{"permission", toString(toolSpec->permission)}}}},
            .auditResult = std::nullopt,
            .auditDiff = std::nullopt});
    }

    if (callItem.name == "run_project_audit") {
        return runProjectAuditExecution(request, callItem, observe);
    }
    if (callItem.name == "re_audit_repaired_project") {
        auto execution = runReAuditRepairedProject(request, callItem);
        if (execution.ok() && observe) {
            for (const auto& observation : execution.value().observations) {
                observe(observation);
            }
        }
        return execution;
    }

    const auto runner = findRunner(callItem.name);
    if (runner == nullptr) {
        return Result<AgentToolExecution>::success(
            AgentToolExecution{.observations = {AgentObservation{
                                   .callId = callItem.id,
                                   .toolName = callItem.name,
                                   .ok = false,
                                   .summary = "工具尚未接入 AgentRuntime: " + callItem.name,
                                   .output = JsonValue::Object{}}},
                               .auditResult = std::nullopt,
                               .auditDiff = std::nullopt});
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
                           .auditResult = std::nullopt,
                           .auditDiff = std::nullopt});
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
    return Result<AgentObservation>::success(std::move(execution.value().observations.back()));
}

} // namespace cc
