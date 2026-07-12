/**
 * @file CompileController.cpp
 * @brief QML 与 C++ Core 的桥接控制器实现。
 */

#include "CompileController.hpp"

#include "AuditResultModels.hpp"
#include "WorkbenchSessionModels.hpp"
#include "cc/agent/AgentCommandRouter.hpp"
#include "cc/agent/AgentRuntime.hpp"
#include "cc/agent/AuditSessionStore.hpp"
#include "cc/agent/ProjectMemory.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/audit/DiffVerifier.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/llm/AdvisoryReconciler.hpp"
#include "cc/llm/BrainAgentLoop.hpp"
#include "cc/llm/LlmBrain.hpp"
#include "cc/llm/LlmProviderProfile.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/report/MarkdownReporter.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/TimeUtil.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] QString normalizedInputPath(const QString& value) {
    const QUrl url{value};
    return url.isLocalFile() ? url.toLocalFile() : value;
}

[[nodiscard]] QString stringText(const std::string& value) {
    return QString::fromStdString(value);
}

[[nodiscard]] QString maskedSecret() {
    return QStringLiteral("********");
}

[[nodiscard]] bool isMaskedSecret(const QString& value) {
    return value == maskedSecret();
}

[[nodiscard]] QString friendlyFailure(const std::string& error, const QString& action) {
    const auto detail = QString::fromStdString(error);
    const auto lower = detail.toLower();
    if (lower.contains("没有返回 json") || lower.contains("invalid json") ||
        (lower.contains("parse") && lower.contains("json"))) {
        return QStringLiteral("%1没有完成：智能助手返回的内容格式不正确。请直接重试；如果反复出现，"
                              "请在设置中更换模型或检查服务地址。")
            .arg(action);
    }
    if (lower.contains("timeout") || lower.contains("timed out")) {
        return QStringLiteral("%1没有完成：连接智能助手超时。请检查网络后重试。").arg(action);
    }
    if (lower.contains("401") || lower.contains("403") || lower.contains("unauthorized") ||
        lower.contains("api key")) {
        return QStringLiteral(
                   "%1没有完成：智能助手的访问凭证无效。请打开设置，检查服务地址、模型和密钥。")
            .arg(action);
    }
    if (lower.contains("connect") || lower.contains("network") || lower.contains("ssl") ||
        lower.contains("https")) {
        return QStringLiteral("%1没有完成：目前无法连接智能助手。你的项目文件没有被修改，请检查网络"
                              "或服务地址后重试。")
            .arg(action);
    }
    return QStringLiteral("%1没有完成。你的原始材料没有被修改，可以重试或查看设置。原因：%2")
        .arg(action, detail);
}

[[nodiscard]] std::size_t auditStageCount() {
    return cc::StagedAuditPipeline::stages().size();
}

[[nodiscard]] bool hasCommonRules(const std::filesystem::path& rulesDir) {
    std::error_code ec;
    return std::filesystem::is_regular_file(rulesDir / "common_rules.json", ec);
}

[[nodiscard]] QString severityLabel(cc::Severity severity) {
    switch (severity) {
    case cc::Severity::Blocker:
        return "必须处理";
    case cc::Severity::Warning:
        return "需要关注";
    case cc::Severity::Info:
        return "提示";
    }
    return "未知";
}

[[nodiscard]] QString friendlyToolName(const std::string& name) {
    if (name == "run_project_audit") {
        return "运行项目规则审计";
    }
    if (name == "inventory_project") {
        return "整理项目文件";
    }
    if (name == "extract_text") {
        return "读取可审计文本";
    }
    if (name == "detect_competition_type") {
        return "判断参赛类别";
    }
    if (name == "build_cpir") {
        return "生成项目画像";
    }
    if (name == "extract_claims") {
        return "提取关键声明";
    }
    if (name == "match_evidence") {
        return "匹配声明证据";
    }
    if (name == "check_consistency") {
        return "检查材料一致性";
    }
    if (name == "run_rules") {
        return "执行审计规则";
    }
    if (name == "calculate_trust_score") {
        return "计算可信评分";
    }
    if (name == "generate_fix_tasks") {
        return "生成补证任务";
    }
    if (name == "generate_repair_plan") {
        return "整理改进方案";
    }
    if (name == "summarize_audit_session") {
        return "查看审计摘要";
    }
    if (name == "list_project_files") {
        return "查看项目文件";
    }
    if (name == "inspect_project_file") {
        return "识别文件类型";
    }
    if (name == "read_text_file") {
        return "读取项目文件";
    }
    if (name == "read_extracted_document") {
        return "阅读抽取文档";
    }
    if (name == "inspect_archive") {
        return "检查项目压缩包";
    }
    if (name == "search_project_text") {
        return "搜索项目内容";
    }
    if (name == "draft_markdown_revision") {
        return "生成材料修订稿";
    }
    if (name == "write_workspace_file") {
        return "保存审计产物";
    }
    if (name == "prepare_repaired_workspace") {
        return "建立修复副本";
    }
    if (name == "apply_repaired_text_edit") {
        return "精确修改项目文件";
    }
    if (name == "create_repaired_text_file") {
        return "新建项目文件";
    }
    if (name == "read_repaired_text_file") {
        return "验证修改结果";
    }
    if (name == "list_workspace_changes") {
        return "汇总项目变更";
    }
    if (name == "re_audit_repaired_project") {
        return "二次审计修复副本";
    }
    return "执行受控审计步骤";
}

[[nodiscard]] QString defectReportText(const cc::AuditResult& result) {
    QStringList lines;
    lines << QStringLiteral("缺点评审完成：材料可信评分 %1/100，还有 %2 分需要通过补材料、"
                            "补证据或修正矛盾来恢复。")
                 .arg(result.trustScore.totalScore)
                 .arg(result.trustScore.trustDebt);
    lines << QStringLiteral("我只列影响交付可信度的问题，不写无关优点。");

    if (result.findings.empty() && result.consistencyIssues.empty() && result.fixTasks.empty()) {
        lines << "当前规则包没有抓到必须处理项或补证任务。建议仍做人工复核后再提交。";
        return lines.join("\n");
    }

    if (!result.findings.empty()) {
        lines << "\n审计发现的问题：";
        const auto limit = std::min<std::size_t>(result.findings.size(), 8U);
        for (std::size_t index = 0U; index < limit; ++index) {
            const auto& finding = result.findings.at(index);
            lines << QStringLiteral("%1. [%2] %3：%4")
                         .arg(static_cast<int>(index + 1U))
                         .arg(severityLabel(finding.severity))
                         .arg(stringText(finding.title))
                         .arg(stringText(finding.reason));
        }
        if (result.findings.size() > limit) {
            lines << QStringLiteral("... 还有 %1 个规则问题已写入报告。")
                         .arg(static_cast<int>(result.findings.size() - limit));
        }
    }

    if (!result.consistencyIssues.empty()) {
        lines << "\n材料一致性缺点：";
        const auto limit = std::min<std::size_t>(result.consistencyIssues.size(), 5U);
        for (std::size_t index = 0U; index < limit; ++index) {
            const auto& issue = result.consistencyIssues.at(index);
            lines << QStringLiteral("%1. [%2] %3；建议：%4")
                         .arg(static_cast<int>(index + 1U))
                         .arg(severityLabel(issue.severity))
                         .arg(stringText(issue.description))
                         .arg(stringText(issue.fixSuggestion));
        }
    }

    if (!result.fixTasks.empty()) {
        lines << "\n优先优化/补证任务：";
        const auto limit = std::min<std::size_t>(result.fixTasks.size(), 6U);
        for (std::size_t index = 0U; index < limit; ++index) {
            const auto& task = result.fixTasks.at(index);
            const auto priority = task.priority == "P0"   ? QStringLiteral("立即处理")
                                  : task.priority == "P1" ? QStringLiteral("尽快处理")
                                                          : stringText(task.priority);
            lines << QStringLiteral("%1. [%2] %3：%4")
                         .arg(static_cast<int>(index + 1U))
                         .arg(priority)
                         .arg(stringText(task.title))
                         .arg(stringText(task.reason));
        }
    }

    lines << "\n需要继续修改时，请切换到 Code 模式并说明要改的文件，或使用 /optimize。"
             "所有改动只进入 repaired project，并提供 diff 和二次审计。";
    return lines.join("\n");
}

void appendParentRuleCandidates(std::vector<std::filesystem::path>& candidates,
                                std::filesystem::path start) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(start, ec)) {
        start = start.parent_path();
    }
    if (start.empty()) {
        return;
    }
    auto current = std::filesystem::absolute(start, ec);
    if (ec) {
        current = std::move(start);
    }
    while (!current.empty()) {
        candidates.push_back(current / "rules");
        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }
}

[[nodiscard]] std::filesystem::path resolveRulesDir(const QString& projectPath) {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("rules");
    appendParentRuleCandidates(candidates, std::filesystem::current_path());

    const auto appDir = std::filesystem::path{QCoreApplication::applicationDirPath().toStdString()};
    appendParentRuleCandidates(candidates, appDir);
    candidates.push_back(appDir / "share" / "contest-compiler" / "rules");
    candidates.push_back(appDir / ".." / "share" / "contest-compiler" / "rules");

    const auto normalizedPath = normalizedInputPath(projectPath).toStdString();
    if (!normalizedPath.empty()) {
        appendParentRuleCandidates(candidates, normalizedPath);
    }

    for (const auto& candidate : candidates) {
        if (hasCommonRules(candidate)) {
            return candidate;
        }
    }
    return "rules";
}

void persistAuditPackage(cc::AuditResult& result) {
    const auto initialized =
        cc::ProjectMemory{}.init(result.context.workspaceRoot, result.cpir.competitionType);
    if (!initialized.ok()) {
        result.context.warnings.push_back("项目约束未能持久化: " + initialized.error());
    }
    const auto saved =
        cc::AuditSessionStore{}.save(result, result.context.workspaceRoot / "audit.json");
    if (!saved.ok()) {
        result.context.warnings.push_back("审计结果未能持久化: " + saved.error());
    }
}

} // namespace

CompileController::CompileController(QObject* parent) : QObject(parent) {
    activeSessionId_ = QString::fromStdString(cc::util::makeSessionId());
    cc::LlmProviderResolver::Environment environment;
    constexpr const char* names[]{
        "ANTHROPIC_API_KEY",   "ANTHROPIC_AUTH_TOKEN", "ANTHROPIC_BASE_URL", "ANTHROPIC_MODEL",
        "OPENAI_API_KEY",      "OPENAI_BASE_URL",      "OPENAI_MODEL",       "DEEPSEEK_API_KEY",
        "DEEPSEEK_AUTH_TOKEN", "DEEPSEEK_BASE_URL",    "DEEPSEEK_MODEL"};
    for (const auto* name : names) {
        const auto* configured = std::getenv(name);
        if (configured != nullptr) {
            environment.emplace(name, configured);
        }
    }
    const auto profile = cc::LlmProviderResolver{}.resolve(environment);
    llmApiKey_ = QString::fromStdString(profile.config.apiKey);
    llmEndpoint_ = QString::fromStdString(profile.config.endpoint);
    llmModel_ = QString::fromStdString(profile.config.model);
    llmProvider_ = QString::fromStdString(profile.config.provider);
    llmApiKeyHeader_ = QString::fromStdString(profile.config.apiKeyHeader);
    llmApiKeyPrefix_ = QString::fromStdString(profile.config.apiKeyPrefix);
    llmApproved_ = false;
}

QString CompileController::projectPath() const {
    return projectPath_;
}

void CompileController::setProjectPath(const QString& value) {
    if (projectPath_ == value) {
        return;
    }
    projectPath_ = value;
    emit projectPathChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

QString CompileController::status() const {
    return status_;
}

bool CompileController::hasAuditResult() const {
    return result_ != nullptr;
}

int CompileController::trustScore() const {
    return result_ != nullptr ? result_->trustScore.totalScore : 0;
}

int CompileController::blockerCount() const {
    return result_ != nullptr ? workbench::blockerCount(*result_) : 0;
}

int CompileController::warningCount() const {
    return result_ != nullptr ? workbench::warningCount(*result_) : 0;
}

QString CompileController::summary() const {
    if (result_ == nullptr) {
        return "尚未运行审计";
    }
    return workbench::summary(*result_);
}

QVariantList CompileController::assets() const {
    return result_ != nullptr ? workbench::assets(*result_) : QVariantList{};
}

QVariantList CompileController::roleDistribution() const {
    return result_ != nullptr ? workbench::roleDistribution(*result_) : QVariantList{};
}

QVariantMap CompileController::cpir() const {
    return result_ != nullptr ? workbench::cpir(*result_) : QVariantMap{};
}

QVariantList CompileController::claimEvidence() const {
    return result_ != nullptr ? workbench::claimEvidence(*result_) : QVariantList{};
}

QVariantList CompileController::consistencyIssues() const {
    return result_ != nullptr ? workbench::consistencyIssues(*result_) : QVariantList{};
}

QVariantList CompileController::findings() const {
    return result_ != nullptr ? workbench::findings(*result_) : QVariantList{};
}

QVariantList CompileController::fixTasks() const {
    return result_ != nullptr ? workbench::fixTasks(*result_) : QVariantList{};
}

QVariantList CompileController::scorePenalties() const {
    return result_ != nullptr ? workbench::scorePenalties(*result_) : QVariantList{};
}

QVariantMap CompileController::projectContext() const {
    return workbench::projectContext(result_.get(), normalizedInputPath(projectPath_));
}

QVariantList CompileController::sessionHistory() const {
    return workbench::sessionHistory(result_.get(), conversation_,
                                     normalizedInputPath(projectPath_));
}

QVariantList CompileController::toolCards() const {
    return workbench::toolCards(result_.get(), auditDiff_, agentRunning_, activeAuditStep_,
                                completedAuditSteps_);
}

QVariantList CompileController::permissionCards() const {
    return workbench::permissionCards(llmApproved_, accessMode_);
}

QVariantList CompileController::artifacts() const {
    return workbench::artifacts(result_.get(), auditDiff_, agentResult_);
}

QString CompileController::agentSummary() const {
    return workbench::agentSummary(result_.get());
}

bool CompileController::agentRunning() const {
    return agentRunning_;
}

int CompileController::agentProgress() const {
    const auto total = static_cast<int>(auditStageCount()) + 1;
    if (agentRunning_ || advisoryRunning_) {
        if (activeAuditStep_ < 0) {
            return 0;
        }
        return std::clamp((completedAuditSteps_ * 100) / total, 0, 99);
    }
    return result_ != nullptr ? 100 : 0;
}

QString CompileController::currentAgentAction() const {
    return currentAgentAction_;
}

QString CompileController::oldAuditPath() const {
    return oldAuditPath_;
}

void CompileController::setOldAuditPath(const QString& value) {
    if (oldAuditPath_ == value) {
        return;
    }
    oldAuditPath_ = value;
    emit diffInputChanged();
}

QString CompileController::newAuditPath() const {
    return newAuditPath_;
}

void CompileController::setNewAuditPath(const QString& value) {
    if (newAuditPath_ == value) {
        return;
    }
    newAuditPath_ = value;
    emit diffInputChanged();
}

QVariantMap CompileController::auditDiff() const {
    return auditDiff_.has_value() ? workbench::auditDiff(*auditDiff_) : QVariantMap{};
}

QVariantMap CompileController::repairWorkspace() const {
    QVariantMap map;
    const bool available = result_ != nullptr && !result_->repairPlan.diffText.empty();
    map["available"] = available;
    if (!available) {
        return map;
    }
    constexpr std::size_t kPreviewBytes = 160U * 1024U;
    const auto& patch = result_->repairPlan.diffText;
    map["patchBytes"] = QVariant::fromValue<qulonglong>(patch.size());
    map["truncated"] = patch.size() > kPreviewBytes;
    map["patchPreview"] =
        QString::fromStdString(patch.substr(0U, std::min(patch.size(), kPreviewBytes)));
    const auto agentRoot = result_->context.workspaceRoot / "agent";
    std::error_code error;
    const auto patchPath = std::filesystem::is_regular_file(agentRoot / "changes.patch", error)
                               ? agentRoot / "changes.patch"
                               : result_->context.workspaceRoot / "changes.patch";
    const auto repairedPath = std::filesystem::is_directory(agentRoot / "repaired-project", error)
                                  ? agentRoot / "repaired-project"
                                  : result_->context.workspaceRoot / "repaired-project";
    map["patchPath"] = QString::fromStdString(patchPath.generic_string());
    map["repairedPath"] = QString::fromStdString(repairedPath.generic_string());
    const std::string marker = "diff --git ";
    int changedFiles = 0;
    std::size_t position = 0U;
    while ((position = patch.find(marker, position)) != std::string::npos) {
        ++changedFiles;
        position += marker.size();
    }
    map["changedFileCount"] = changedFiles;
    return map;
}

QString CompileController::llmApiKey() const {
    return llmApiKey_.isEmpty() ? QString{} : maskedSecret();
}

void CompileController::setLlmApiKey(const QString& value) {
    const auto trimmed = value.trimmed();
    if (isMaskedSecret(trimmed)) {
        return;
    }
    if (llmApiKey_ == trimmed) {
        return;
    }
    llmApiKey_ = trimmed;
    llmApproved_ = false;
    emit llmConfigChanged();
    emit workspaceChanged();
}

QString CompileController::llmEndpoint() const {
    return llmEndpoint_;
}

void CompileController::setLlmEndpoint(const QString& value) {
    if (llmEndpoint_ == value) {
        return;
    }
    llmEndpoint_ = value;
    llmApproved_ = false;
    emit llmConfigChanged();
    emit workspaceChanged();
}

QString CompileController::llmModel() const {
    return llmModel_;
}

void CompileController::setLlmModel(const QString& value) {
    if (llmModel_ == value) {
        return;
    }
    llmModel_ = value;
    emit llmConfigChanged();
}

bool CompileController::llmApproved() const {
    return llmApproved_;
}

void CompileController::setLlmApproved(bool value) {
    const bool approved = value && !llmApiKey_.isEmpty() &&
                          llmEndpoint_.trimmed().startsWith("https://") &&
                          !llmModel_.trimmed().isEmpty();
    if (llmApproved_ == approved) {
        return;
    }
    llmApproved_ = approved;
    if (value && !approved) {
        status_ = "LLM 授权失败：请先配置 API key、HTTPS endpoint 和模型";
        emit statusChanged();
    }
    emit llmConfigChanged();
    emit workspaceChanged();
}

QString CompileController::agentResult() const {
    return agentResult_;
}

QString CompileController::agentTrace() const {
    return agentTrace_;
}

QString CompileController::accessMode() const {
    return accessMode_;
}

void CompileController::setAccessMode(const QString& value) {
    const auto key = value.trimmed().toLower();
    QString normalized = "ask";
    if (key == "bypass" || key == "bypasspermissions" || key == "bypass-permissions" ||
        key == "expanded-read") {
        normalized = "bypass";
    } else if (key == "code" || key == "accept-edits" || key == "acceptedits") {
        normalized = "code";
    } else if (key == "plan" || key == "manual" || key == "readonly" || key == "read-only") {
        normalized = "plan";
    } else if (key == "ask" || key == "auto" || key == "default" || key == "sandbox") {
        normalized = "ask";
    }
    if (accessMode_ == normalized) {
        return;
    }
    accessMode_ = normalized;
    emit accessModeChanged();
    emit workspaceChanged();
}

void CompileController::archiveCurrentSession() {
    const bool hasConversation = std::any_of(
        conversation_.begin(), conversation_.end(), [](const workbench::SessionMessage& message) {
            return message.kind == "user" || message.kind == "assistant" ||
                   message.kind == "artifact";
        });
    if (normalizedInputPath(projectPath_).trimmed().isEmpty() && result_ == nullptr &&
        !hasConversation) {
        return;
    }

    SavedSession saved;
    saved.id = activeSessionId_;
    saved.projectPath = std::move(projectPath_);
    saved.oldAuditPath = std::move(oldAuditPath_);
    saved.newAuditPath = std::move(newAuditPath_);
    saved.status = std::move(status_);
    saved.accessMode = std::move(accessMode_);
    saved.agentResult = std::move(agentResult_);
    saved.agentTrace = std::move(agentTrace_);
    saved.pendingPlanGoal = std::move(pendingPlanGoal_);
    saved.compactedContext = std::move(compactedContext_);
    saved.selectedFilePreview = std::move(selectedFilePreview_);
    saved.result = std::move(result_);
    saved.baselineResult = std::move(baselineResult_);
    saved.auditDiff = std::move(auditDiff_);
    saved.advisory = std::move(advisory_);
    saved.conversation = std::move(conversation_);
    savedSessions_.insert(savedSessions_.begin(), std::move(saved));
    constexpr std::size_t kMaximumInactiveSessions = 12U;
    if (savedSessions_.size() > kMaximumInactiveSessions) {
        savedSessions_.resize(kMaximumInactiveSessions);
    }
}

void CompileController::resetActiveSession(bool addGreeting) {
    pendingComposerMessages_.clear();
    projectPath_.clear();
    oldAuditPath_.clear();
    newAuditPath_.clear();
    result_.reset();
    baselineResult_.reset();
    auditDiff_.reset();
    advisory_.reset();
    advisoryRunning_ = false;
    agentResult_.clear();
    agentTrace_.clear();
    accessMode_ = "ask";
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    pendingPlanGoal_.clear();
    compactedContext_.clear();
    selectedFilePreview_.clear();
    conversation_.clear();
    activeSessionId_ = QString::fromStdString(cc::util::makeSessionId());
    status_ = "已开始新任务，等待添加项目文件";
    if (addGreeting) {
        conversation_.push_back({"系统", "已开始新任务。请添加完整项目文件夹、项目压缩包或单个项目文件。",
                                 "会话", "system", QString{}, true});
    }
}

void CompileController::emitFullSessionState() {
    emit projectPathChanged();
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit advisoryChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit selectedFilePreviewChanged();
    emit agentStateChanged();
    emit accessModeChanged();
    emit diffInputChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::selectProject(const QString& urlOrPath) {
    const auto path = normalizedInputPath(urlOrPath).trimmed();
    if (path.isEmpty()) {
        return;
    }
    if (agentRunning_ || advisoryRunning_) {
        conversation_.push_back({"系统", "当前审计仍在运行，完成后再切换新项目。", "项目导入",
                                 "system", QString{}, false});
        emit sessionChanged();
        return;
    }
    if (!projectPath_.trimmed().isEmpty() && normalizedInputPath(projectPath_).trimmed() != path) {
        archiveCurrentSession();
        resetActiveSession(false);
    }
    setProjectPath(path);
    result_.reset();
    baselineResult_.reset();
    auditDiff_.reset();
    advisory_.reset();
    agentResult_.clear();
    agentTrace_.clear();
    pendingPlanGoal_.clear();
    selectedFilePreview_.clear();
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    const auto selectedName = QFileInfo(path).fileName();
    conversation_.push_back(
        {"用户",
         QStringLiteral("已添加项目：%1").arg(selectedName.isEmpty() ? "项目文件" : selectedName),
         QFileInfo(path).isDir() ? "项目文件夹" : "项目文件", "user", QString{}, true});
    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    status_ = brainReady ? "大模型审计助手正在分析项目" : "正在启动本地规则审计";
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit advisoryChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit selectedFilePreviewChanged();
    emit workspaceChanged();
    emit sessionChanged();
    if (brainReady) {
        startDeferredAgentConversation(
            "请审查当前项目。先调用 run_project_audit 获取确定性规则、证据匹配和评分结果，"
            "再根据检查结果继续读取真实源码、配置或办公文件，最后回答主要缺点和下一步。",
            "项目导入", false);
        return;
    }
    runAudit();
}

void CompileController::newSession() {
    if (agentRunning_ || advisoryRunning_) {
        conversation_.push_back({"系统", "当前审计仍在运行，完成后才能开始新任务。", "会话",
                                 "system", QString{}, false});
        emit sessionChanged();
        return;
    }
    archiveCurrentSession();
    resetActiveSession(true);
    emitFullSessionState();
}

QVariantList CompileController::sessionList() const {
    QVariantList items;
    const auto makeItem = [](const QString& id, const QString& projectPath,
                             const std::shared_ptr<cc::AuditResult>& result, bool active,
                             bool running) {
        QVariantMap item;
        item["sessionId"] = id;
        if (result != nullptr) {
            item["title"] = QString::fromStdString(result->context.projectName.empty()
                                                       ? result->context.sessionId
                                                       : result->context.projectName);
            item["subtitle"] = QStringLiteral("评分 %1 · 必须处理 %2")
                                   .arg(result->trustScore.totalScore)
                                   .arg(workbench::blockerCount(*result));
        } else {
            const auto path = normalizedInputPath(projectPath).trimmed();
            const auto fileName = QFileInfo(path).fileName();
            item["title"] = fileName.isEmpty() ? (path.isEmpty() ? "新任务" : path) : fileName;
            item["subtitle"] = running ? "审计进行中" : "待审计";
        }
        item["active"] = active;
        return item;
    };

    const bool activeMeaningful = !normalizedInputPath(projectPath_).trimmed().isEmpty() ||
                                  result_ != nullptr || !conversation_.empty();
    if (activeMeaningful) {
        items.push_back(makeItem(activeSessionId_, projectPath_, result_, true, agentRunning_));
    }
    for (const auto& saved : savedSessions_) {
        items.push_back(makeItem(saved.id, saved.projectPath, saved.result, false, false));
    }
    return items;
}

void CompileController::activateSession(const QString& sessionId) {
    if (agentRunning_ || advisoryRunning_ || sessionId.isEmpty() || sessionId == activeSessionId_) {
        return;
    }
    const auto selected =
        std::find_if(savedSessions_.begin(), savedSessions_.end(),
                     [&](const SavedSession& item) { return item.id == sessionId; });
    if (selected == savedSessions_.end()) {
        return;
    }
    auto restored = std::move(*selected);
    savedSessions_.erase(selected);
    archiveCurrentSession();

    activeSessionId_ = std::move(restored.id);
    projectPath_ = std::move(restored.projectPath);
    oldAuditPath_ = std::move(restored.oldAuditPath);
    newAuditPath_ = std::move(restored.newAuditPath);
    status_ = std::move(restored.status);
    accessMode_ = std::move(restored.accessMode);
    agentResult_ = std::move(restored.agentResult);
    agentTrace_ = std::move(restored.agentTrace);
    pendingPlanGoal_ = std::move(restored.pendingPlanGoal);
    compactedContext_ = std::move(restored.compactedContext);
    selectedFilePreview_ = std::move(restored.selectedFilePreview);
    result_ = std::move(restored.result);
    baselineResult_ = std::move(restored.baselineResult);
    auditDiff_ = std::move(restored.auditDiff);
    advisory_ = std::move(restored.advisory);
    conversation_ = std::move(restored.conversation);
    activeAuditStep_ = -1;
    completedAuditSteps_ = result_ != nullptr ? static_cast<int>(auditStageCount()) : 0;
    currentAgentAction_.clear();
    emitFullSessionState();
}

bool CompileController::advisoryRunning() const {
    return advisoryRunning_;
}

QVariantMap CompileController::advisory() const {
    QVariantMap map;
    if (!advisory_.has_value()) {
        map["available"] = false;
        return map;
    }
    const auto& adv = *advisory_;
    map["available"] = true;
    map["finalScore"] = adv.finalScore;
    map["suggestedScore"] = adv.suggestedScore;
    map["scoreGap"] = adv.scoreGap;
    map["confirmedCount"] = static_cast<int>(adv.confirmedCount);
    map["unverifiedCount"] = static_cast<int>(adv.unverifiedCount);
    map["conflictingCount"] = static_cast<int>(adv.conflictingCount);
    map["summary"] = QString::fromStdString(adv.summary);
    QVariantList items;
    for (const auto& item : adv.items) {
        QVariantMap entry;
        entry["title"] = QString::fromStdString(item.advisory.title);
        entry["reason"] = QString::fromStdString(item.advisory.reason);
        entry["suggestion"] = QString::fromStdString(item.advisory.suggestion);
        entry["verdict"] = QString::fromStdString(cc::toString(item.verdict));
        entry["reconciliation"] = QString::fromStdString(item.reconciliation);
        items.push_back(entry);
    }
    map["items"] = items;
    return map;
}

QVariantMap CompileController::selectedFilePreview() const {
    return selectedFilePreview_;
}

void CompileController::runAdvisory() {
    if (advisoryRunning_ || agentRunning_) {
        return;
    }
    if (result_ == nullptr) {
        status_ = "请先运行审计，再让 LLM 研判";
        conversation_.push_back({"系统", status_, "混合研判", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }
    if (!llmApproved_ || llmApiKey_.isEmpty()) {
        status_ = "混合研判需要在 LLM Brain 页填入 API key 并授权联网";
        conversation_.push_back({"系统", status_, "混合研判", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    advisoryRunning_ = true;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeCancellation_ = cancellation;
    status_ = "LLM 正在研判，稍后由确定性规则校验";
    conversation_.push_back(
        {"计划",
         "混合研判：LLM 先基于审计上下文给出风险判断和评分建议，随后由确定性规则和证据"
         "逐条校验，冲突项会被降级并标注，最终评分仍以规则引擎为准。",
         "混合研判"});
    emit statusChanged();
    emit advisoryChanged();
    emit sessionChanged();

    cc::LlmConfig config;
    config.apiKey = llmApiKey_.toStdString();
    config.endpoint = llmEndpoint_.toStdString();
    config.model = llmModel_.toStdString();
    config.provider = llmProvider_.toStdString();
    config.apiKeyHeader = llmApiKeyHeader_.toStdString();
    config.apiKeyPrefix = llmApiKeyPrefix_.toStdString();
    config.allowNetwork = true;
    config.allowLlm = true;
    config.isCancelled = [cancellation]() { return cancellation->load(); };
    auto auditSnapshot = result_;
    const auto sessionId = activeSessionId_;
    const QPointer<CompileController> guard{this};
    auto* worker = QThread::create([guard, config = std::move(config), auditSnapshot, sessionId,
                                    cancellation]() mutable {
        auto proposed = cc::LlmBrain{}.requestAuditAdvisory(config, *auditSnapshot);
        auto outcome = std::make_shared<cc::Result<cc::ReconciledAdvisory>>(
            proposed.ok()
                ? cc::Result<cc::ReconciledAdvisory>::success(
                      cc::AdvisoryReconciler{}.reconcile(proposed.value(), *auditSnapshot))
                : cc::Result<cc::ReconciledAdvisory>::failure(proposed.error()));
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, outcome, sessionId, cancellation]() mutable {
                if (guard.isNull() || cancellation->load() ||
                    guard->activeSessionId_ != sessionId ||
                    guard->activeCancellation_ != cancellation) {
                    return;
                }
                guard->activeCancellation_.reset();
                guard->advisoryRunning_ = false;
                if (!outcome->ok()) {
                    guard->status_ = QString::fromStdString(outcome->error());
                    guard->conversation_.push_back(
                        {"系统", guard->status_, "研判失败", "system", QString{}, false});
                } else {
                    guard->advisory_ = std::move(outcome->value());
                    guard->status_ = "混合研判完成";
                    guard->conversation_.push_back(
                        {"智能体", QString::fromStdString(guard->advisory_->summary), "混合研判",
                         "assistant", QString{}, guard->advisory_->conflictingCount == 0});
                    for (const auto& item : guard->advisory_->items) {
                        const bool ok = item.verdict != cc::AdvisoryVerdict::Conflicting;
                        guard->conversation_.push_back(
                            {"工具", QString::fromStdString(item.advisory.title),
                             QString::fromStdString(cc::toString(item.verdict)), "tool",
                             QString::fromStdString(item.reconciliation), ok});
                    }
                }
                emit guard->statusChanged();
                emit guard->advisoryChanged();
                emit guard->workspaceChanged();
                emit guard->sessionChanged();
                guard->flushQueuedComposerMessage();
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::runGeneralAssistant(const QString& message, const QString& context,
                                            bool appendUserMessage) {
    const auto trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (appendUserMessage) {
        conversation_.push_back({"用户", trimmed, context, "user", QString{}, true});
    }

    if (!llmApproved_ || llmApiKey_.isEmpty()) {
        status_ = "常规问答需要先在设置中配置 LLM Brain";
        conversation_.push_back(
            {"智能体",
             "我可以作为常规问答助手，也可以专门评审竞赛项目。当前没有配置 LLM Brain，所以"
             "不能生成开放式回答；你可以先拖入项目让我做规则评审，或在设置里填入 API key 后再问。",
             "常规问答", "assistant", QString{}, true});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    cc::LlmConfig config;
    config.apiKey = llmApiKey_.toStdString();
    config.endpoint = llmEndpoint_.toStdString();
    config.model = llmModel_.toStdString();
    config.provider = llmProvider_.toStdString();
    config.apiKeyHeader = llmApiKeyHeader_.toStdString();
    config.apiKeyPrefix = llmApiKeyPrefix_.toStdString();
    config.allowNetwork = true;
    config.allowLlm = true;
    auto cancellation = activeCancellation_;
    if (!cancellation) {
        cancellation = std::make_shared<std::atomic_bool>(false);
        activeCancellation_ = cancellation;
    }
    config.isCancelled = [cancellation]() { return cancellation->load(); };

    std::vector<cc::LlmMessage> messages{
        {.role = "system",
         .content = "你是竞赛项目可信评审平台中的常规问答助手。回答要清楚、直接；涉及项目评审时"
                    "提醒用户拖入项目以便结合规则和证据，不要伪造材料或评分。"}};
    if (!compactedContext_.isEmpty()) {
        messages.push_back({.role = "system", .content = compactedContext_.toStdString()});
    }
    const auto historyStart = conversation_.size() > 12U ? conversation_.size() - 12U : 0U;
    for (std::size_t index = historyStart; index < conversation_.size(); ++index) {
        const auto& item = conversation_.at(index);
        const auto kind = item.kind.trimmed().toLower();
        if ((kind != "user" && kind != "assistant") || item.text.trimmed().isEmpty()) {
            continue;
        }
        messages.push_back(
            {.role = kind.toStdString(), .content = item.text.trimmed().toStdString()});
    }
    if (messages.back().role != "user" || messages.back().content != trimmed.toStdString()) {
        messages.push_back({.role = "user", .content = trimmed.toStdString()});
    }
    status_ = "正在生成回答";
    currentAgentAction_ = "生成回答";
    brainWorkerRunning_ = true;
    emit statusChanged();
    emit agentStateChanged();
    emit sessionChanged();

    const auto sessionId = activeSessionId_;
    const QPointer<CompileController> guard{this};
    auto* worker =
        QThread::create([guard, config = std::move(config), messages = std::move(messages),
                         sessionId, cancellation]() mutable {
            auto response = std::make_shared<cc::Result<cc::LlmResponse>>(
                cc::LlmBrain{}.complete(config, messages));
            if (guard.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                guard,
                [guard, response, sessionId, cancellation]() mutable {
                    if (guard.isNull() || cancellation->load() ||
                        guard->activeSessionId_ != sessionId ||
                        guard->activeCancellation_ != cancellation) {
                        return;
                    }
                    guard->brainWorkerRunning_ = false;
                    if (!response->ok()) {
                        guard->status_ = QString::fromStdString(response->error());
                        guard->conversation_.push_back(
                            {"系统", guard->status_, "常规问答", "system", QString{}, false});
                    } else {
                        guard->status_ = "常规问答已完成";
                        guard->conversation_.push_back(
                            {"智能体", QString::fromStdString(response->value().content),
                             "常规问答", "assistant", QString{}, true});
                    }
                    emit guard->statusChanged();
                    emit guard->sessionChanged();
                    guard->finishDeferredAgentConversation();
                },
                Qt::QueuedConnection);
        });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::runAudit() {
    if (agentRunning_ || advisoryRunning_) {
        status_ = "审计正在进行";
        emit statusChanged();
        return;
    }
    if (normalizedInputPath(projectPath_).trimmed().isEmpty()) {
        status_ = "请先添加项目文件";
        conversation_.push_back(
            {"智能体", "先添加完整项目文件夹、项目压缩包或单个项目文件，我再开始检查。", "等待项目"});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    cc::AuditOptions options;
    options.rulesDir = resolveRulesDir(projectPath_);
    const auto normalizedPath = normalizedInputPath(projectPath_);

    result_.reset();
    baselineResult_.reset();
    auditDiff_.reset();
    advisory_.reset();
    compactedContext_.clear();
    agentResult_.clear();
    agentTrace_.clear();
    selectedFilePreview_.clear();
    agentRunning_ = true;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeCancellation_ = cancellation;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_ = "建立安全工作副本";
    status_ = "正在建立安全工作副本";
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit advisoryChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit selectedFilePreviewChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
    const auto sessionId = activeSessionId_;
    const QPointer<CompileController> guard{this};
    auto* worker = QThread::create(
        [guard, path = normalizedPath.toStdString(), options, sessionId, cancellation]() mutable {
            if (cancellation->load()) {
                return;
            }
            cc::StagedAuditPipeline pipeline;
            auto begun = pipeline.begin(path, options);
            if (!begun.ok()) {
                auto failure = std::make_shared<cc::Result<cc::AuditResult>>(
                    cc::Result<cc::AuditResult>::failure(begun.error()));
                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(
                        guard,
                        [guard, failure, sessionId, cancellation]() mutable {
                            if (!guard.isNull() && !cancellation->load() &&
                                guard->activeSessionId_ == sessionId &&
                                guard->activeCancellation_ == cancellation) {
                                guard->completeAuditRun(std::move(*failure));
                            }
                        },
                        Qt::QueuedConnection);
                }
                return;
            }

            std::size_t stageIndex = 0U;
            while (pipeline.hasNext()) {
                if (cancellation->load()) {
                    return;
                }
                auto observed = pipeline.advance();
                if (!observed.ok()) {
                    auto failure = std::make_shared<cc::Result<cc::AuditResult>>(
                        cc::Result<cc::AuditResult>::failure(observed.error()));
                    if (!guard.isNull()) {
                        QMetaObject::invokeMethod(
                            guard,
                            [guard, failure, sessionId, cancellation]() mutable {
                                if (!guard.isNull() && !cancellation->load() &&
                                    guard->activeSessionId_ == sessionId &&
                                    guard->activeCancellation_ == cancellation) {
                                    guard->completeAuditRun(std::move(*failure));
                                }
                            },
                            Qt::QueuedConnection);
                    }
                    return;
                }
                auto sharedObservation = std::make_shared<cc::AgentObservation>(observed.value());
                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(
                        guard,
                        [guard, sharedObservation, stageIndex, sessionId, cancellation]() {
                            if (!guard.isNull() && !cancellation->load() &&
                                guard->activeSessionId_ == sessionId &&
                                guard->activeCancellation_ == cancellation) {
                                guard->applyAuditStage(stageIndex, *sharedObservation);
                            }
                        },
                        Qt::QueuedConnection);
                }
                ++stageIndex;
            }
            auto finished = pipeline.finish();
            if (finished.ok()) {
                persistAuditPackage(finished.value());
            }
            auto outcome = std::make_shared<cc::Result<cc::AuditResult>>(std::move(finished));
            if (!guard.isNull()) {
                QMetaObject::invokeMethod(
                    guard,
                    [guard, outcome, sessionId, cancellation]() mutable {
                        if (!guard.isNull() && !cancellation->load() &&
                            guard->activeSessionId_ == sessionId &&
                            guard->activeCancellation_ == cancellation) {
                            guard->completeAuditRun(std::move(*outcome));
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::applyAuditStage(std::size_t stageIndex,
                                        const cc::AgentObservation& observation) {
    if (!agentRunning_) {
        return;
    }
    const auto title = stringText(observation.output.at("title").asString());
    activeAuditStep_ = static_cast<int>(stageIndex);
    completedAuditSteps_ = static_cast<int>(stageIndex + 1U);
    currentAgentAction_ = title;
    status_ = QStringLiteral("已%1").arg(title);
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
}

void CompileController::completeAuditRun(cc::Result<cc::AuditResult> result) {
    activeCancellation_.reset();
    if (!result.ok()) {
        agentRunning_ = false;
        currentAgentAction_ = "审计失败";
        status_ = friendlyFailure(result.error(), "项目检查");
        conversation_.push_back({"系统", status_, "审计失败", "system", QString{}, false});
        emit statusChanged();
        emit agentStateChanged();
        emit workspaceChanged();
        emit sessionChanged();
        flushQueuedComposerMessage();
        return;
    }
    result_ = std::make_shared<cc::AuditResult>(std::move(result.value()));
    baselineResult_ = result_;
    agentRunning_ = false;
    completedAuditSteps_ = static_cast<int>(auditStageCount());
    currentAgentAction_ = "审计完成";
    status_ = result_->context.warnings.empty()
                  ? "检查完成：先处理红色的“必须处理”，再查看黄色的“需要关注”"
                  : "检查完成：部分材料未能完整读取，请查看右侧提示";
    conversation_.push_back(
        {"工具", "已完成材料整理、证据匹配、规则检查和评分。",
         QStringLiteral("会话 %1").arg(stringText(result_->context.sessionId))});
    conversation_.push_back(
        {"智能体", defectReportText(*result_), "缺点评审报告", "assistant", QString{}, true});
    conversation_.push_back({"产物", "打开可信评分、材料、证据、风险和修复任务。", "完整审计结果",
                             "artifact", QString{}, true, "dashboard"});
    if (!result_->context.warnings.empty()) {
        QStringList warnings;
        constexpr std::size_t kDisplayedWarnings = 6U;
        const auto count = std::min(result_->context.warnings.size(), kDisplayedWarnings);
        for (std::size_t index = 0U; index < count; ++index) {
            warnings.push_back(QStringLiteral("• %1").arg(
                QString::fromStdString(result_->context.warnings.at(index))));
        }
        if (result_->context.warnings.size() > count) {
            warnings.push_back(QStringLiteral("• 另有 %1 条提示可在项目上下文中查看")
                                   .arg(result_->context.warnings.size() - count));
        }
        conversation_.push_back(
            {"系统", warnings.join("\n"), "非阻断导入提示", "system", QString{}, true});
    }
    emit statusChanged();
    emit agentStateChanged();
    emit resultChanged();
    emit workspaceChanged();
    emit sessionChanged();
    flushQueuedComposerMessage();
}

void CompileController::runDiff() {
    if (agentRunning_ || advisoryRunning_) {
        return;
    }
    if (oldAuditPath_.isEmpty() || newAuditPath_.isEmpty()) {
        status_ = "请先填写两份 audit.json 路径";
        emit statusChanged();
        return;
    }
    agentRunning_ = true;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeCancellation_ = cancellation;
    currentAgentAction_ = "比较两次审计";
    status_ = "正在后台比较两份审计数据包";
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();

    const auto oldPath = normalizedInputPath(oldAuditPath_).toStdString();
    const auto newPath = normalizedInputPath(newAuditPath_).toStdString();
    const auto sessionId = activeSessionId_;
    const QPointer<CompileController> guard{this};
    auto* worker = QThread::create([guard, oldPath, newPath, sessionId, cancellation]() {
        if (cancellation->load()) {
            return;
        }
        auto outcome = std::make_shared<cc::Result<cc::AuditDiff>>(
            cc::DiffVerifier{}.diffFiles(oldPath, newPath));
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, outcome, sessionId, cancellation]() mutable {
                if (guard.isNull() || cancellation->load() ||
                    guard->activeSessionId_ != sessionId ||
                    guard->activeCancellation_ != cancellation) {
                    return;
                }
                guard->activeCancellation_.reset();
                guard->agentRunning_ = false;
                guard->currentAgentAction_.clear();
                if (!outcome->ok()) {
                    guard->status_ = QString::fromStdString(outcome->error());
                } else {
                    guard->auditDiff_ = std::move(outcome->value());
                    guard->status_ = "二次审计差分完成";
                    guard->conversation_.push_back(
                        {"工具", "已基于两份审计数据包生成二次审计差分。", "差分完成", "tool",
                         QString::fromStdString(guard->auditDiff_->summary), true});
                    emit guard->auditDiffChanged();
                }
                emit guard->statusChanged();
                emit guard->agentStateChanged();
                emit guard->workspaceChanged();
                emit guard->sessionChanged();
                guard->flushQueuedComposerMessage();
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::runBrainTask(const QString& goal) {
    const auto trimmed = goal.trimmed().isEmpty()
                             ? QStringLiteral("请接管当前竞赛项目，翻阅材料并给出可信审计下一步。")
                             : goal.trimmed();
    startDeferredAgentConversation(trimmed, "Brain 任务", true);
}

QString CompileController::accessModeLabel() const {
    if (accessMode_ == "bypass") {
        return "扩展读取模式";
    }
    if (accessMode_ == "code") {
        return "Code 模式";
    }
    if (accessMode_ == "plan") {
        return "Plan 模式";
    }
    return "Ask 模式";
}

QString CompileController::sessionStatusText() const {
    QStringList lines;
    const auto normalized = normalizedInputPath(projectPath_).trimmed();
    lines << QStringLiteral("权限模式：%1").arg(accessModeLabel());
    lines << QStringLiteral("项目文件：%1").arg(normalized.isEmpty() ? "未选择" : normalized);
    lines << QStringLiteral("LLM Brain：%1")
                 .arg(llmApproved_ && !llmApiKey_.isEmpty() ? "已授权" : "本地受控");
    lines << QStringLiteral("当前状态：%1").arg(status_);
    if (result_ != nullptr) {
        lines << QStringLiteral("审计结果：评分 %1/100，必须处理 %2，需要关注 %3，补证任务 %4")
                     .arg(result_->trustScore.totalScore)
                     .arg(workbench::blockerCount(*result_))
                     .arg(workbench::warningCount(*result_))
                     .arg(result_->fixTasks.size());
    } else {
        lines << "审计结果：尚未生成";
    }
    return lines.join("\n");
}

QString CompileController::compactedContextText() const {
    QStringList lines;
    lines << "上下文摘要";
    lines << QStringLiteral("权限模式：%1").arg(accessModeLabel());
    const auto normalized = normalizedInputPath(projectPath_).trimmed();
    if (!normalized.isEmpty()) {
        lines << QStringLiteral("当前材料：%1").arg(normalized);
    }
    if (result_ == nullptr) {
        lines << "尚未运行审计；下一步通常是 /audit。";
        return lines.join("\n");
    }
    lines << agentSummary();
    lines << QStringLiteral("资产 %1 个，声明 %2 条，证据匹配 %3 条，风险 %4 个。")
                 .arg(result_->inventory.assets.size())
                 .arg(result_->claims.size())
                 .arg(result_->evidenceMatches.size())
                 .arg(result_->findings.size());
    if (!result_->findings.empty()) {
        lines << QStringLiteral("首个风险：%1")
                     .arg(QString::fromStdString(result_->findings.front().reason));
    }
    if (!result_->fixTasks.empty()) {
        lines << QStringLiteral("优先补证：%1")
                     .arg(QString::fromStdString(result_->fixTasks.front().title));
    }
    return lines.join("\n");
}

void CompileController::previewAgentPlan(const QString& message, const QString& context) {
    previewAgentPlan(message, context, true);
}

void CompileController::previewAgentPlan(const QString& message, const QString& context,
                                         bool appendUserMessage) {
    const auto goal = message.trimmed().isEmpty()
                          ? QStringLiteral("为当前竞赛项目生成下一步审计计划。")
                          : message.trimmed();
    if (appendUserMessage) {
        conversation_.push_back({"用户", goal, context});
    }
    pendingPlanGoal_ = goal;

    QStringList steps;
    steps << QStringLiteral("目标：%1").arg(goal);
    steps << "1. 固定读取当前项目状态、权限模式和已有审计结果。";
    if (result_ == nullptr) {
        steps << "2. 先运行可信审计，生成资产清单、项目画像、声明证据和规则风险。";
        steps << "3. 审计完成后再追问风险解释、补证优先级或报告导出。";
    } else {
        steps << "2. 压缩审计摘要，优先查看必须处理项、证据缺口和补证任务。";
        steps << "3. 按需调用只读工具翻阅文件、搜索证据；需要写入时只生成工作区产物。";
    }
    steps << "4. 等你确认后再切换到 ask/code 模式执行受控工具；只有明确需要时才切换 bypass。";

    status_ = "计划已生成，等待确认";
    conversation_.push_back({"计划", steps.join("\n"), "计划模式", "plan", QString{}, true});
    emit statusChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::approvePendingPlan() {
    if (agentRunning_ || advisoryRunning_) {
        conversation_.push_back(
            {"系统", "当前任务仍在运行，请稍后再执行计划。", "计划", "system", QString{}, false});
        emit sessionChanged();
        return;
    }
    if (pendingPlanGoal_.isEmpty()) {
        conversation_.push_back(
            {"系统", "当前没有等待执行的计划。", "计划", "system", QString{}, false});
        emit sessionChanged();
        return;
    }

    const auto goal = pendingPlanGoal_;
    pendingPlanGoal_.clear();
    setAccessMode("ask");
    conversation_.push_back(
        {"系统", "计划已批准，审计助手开始执行。", "计划", "system", QString{}, true});
    emit sessionChanged();
    startDeferredAgentConversation(goal, "执行已批准计划", false);
}

void CompileController::rewindLastTurn() {
    if (agentRunning_ || advisoryRunning_) {
        conversation_.push_back(
            {"系统", "当前任务仍在运行，完成后才能回退。", "会话", "system", QString{}, false});
        emit sessionChanged();
        return;
    }

    const auto userTurn = std::find_if(conversation_.rbegin(), conversation_.rend(),
                                       [](const workbench::SessionMessage& message) {
                                           return message.kind == "user" || message.role == "用户";
                                       });
    if (userTurn == conversation_.rend()) {
        conversation_.push_back({"系统", "没有可回退的对话。", "会话", "system", QString{}, false});
        emit sessionChanged();
        return;
    }

    const auto eraseStart = std::prev(userTurn.base());
    if (eraseStart->context == "项目导入") {
        conversation_.push_back({"系统", "项目导入记录不能通过回退删除；请点击“新任务”重新开始。",
                                 "会话", "system", QString{}, false});
        emit sessionChanged();
        return;
    }

    conversation_.erase(eraseStart, conversation_.end());
    pendingPlanGoal_.clear();
    agentResult_.clear();
    agentTrace_.clear();
    conversation_.push_back(
        {"系统", "已回退最近一轮对话，可以重新输入。", "会话", "system", QString{}, true});
    status_ = "已回退最近一轮对话";
    emit statusChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::previewProjectFile(const QString& relativePath) {
    if (result_ == nullptr) {
        selectedFilePreview_.clear();
        selectedFilePreview_["error"] = "请先完成项目审计，再预览文件。";
        emit selectedFilePreviewChanged();
        return;
    }

    const auto requested = std::filesystem::path(relativePath.toStdString()).lexically_normal();
    const auto asset =
        std::find_if(result_->inventory.assets.begin(), result_->inventory.assets.end(),
                     [&requested](const cc::ProjectAsset& candidate) {
                         return candidate.relativePath.lexically_normal() == requested;
                     });
    if (asset == result_->inventory.assets.end()) {
        selectedFilePreview_.clear();
        selectedFilePreview_["error"] = "文件不在当前审计资产清单中。";
        emit selectedFilePreviewChanged();
        return;
    }

    QString content;
    QString extractionStatus = asset->auditable ? "可审计" : "仅显示文件信息";
    const auto document = std::find_if(result_->corpus.begin(), result_->corpus.end(),
                                       [&asset](const cc::TextDocument& candidate) {
                                           return candidate.sourceFile.lexically_normal() ==
                                                  asset->relativePath.lexically_normal();
                                       });
    if (document != result_->corpus.end()) {
        content = QString::fromStdString(document->text);
        extractionStatus = QString::fromStdString(document->status);
    } else if (asset->auditable) {
        content = QString::fromStdString(cc::util::readFileLimited(asset->absolutePath, 48000U));
    }
    if (content.trimmed().isEmpty()) {
        content = asset->auditable ? "该文件没有提取出可显示文本，可能是扫描件或二进制格式。"
                                   : "该文件不适合直接文本预览；审计仍会保留格式、角色和风险信息。";
    }

    selectedFilePreview_.clear();
    selectedFilePreview_["path"] = relativePath;
    selectedFilePreview_["name"] = QString::fromStdString(asset->fileName);
    selectedFilePreview_["format"] = QString::fromStdString(asset->format);
    selectedFilePreview_["role"] = QString::fromStdString(cc::toString(asset->role));
    selectedFilePreview_["size"] = QVariant::fromValue<qulonglong>(asset->sizeBytes);
    selectedFilePreview_["risk"] = workbench::riskFlags(asset->riskFlags);
    selectedFilePreview_["status"] = extractionStatus;
    constexpr qsizetype previewLimit = 24000;
    selectedFilePreview_["contentLength"] = content.size();
    selectedFilePreview_["truncated"] = content.size() > previewLimit;
    selectedFilePreview_["content"] = content.left(previewLimit);
    status_ = QStringLiteral("已打开 %1 的预览").arg(relativePath);
    emit statusChanged();
    emit selectedFilePreviewChanged();
}

void CompileController::clearSelectedFilePreview() {
    if (selectedFilePreview_.isEmpty()) {
        return;
    }
    selectedFilePreview_.clear();
    emit selectedFilePreviewChanged();
}

void CompileController::runAgentConversation(const QString& message, const QString& context,
                                             bool appendUserMessage) {
    if (agentRunning_ && appendUserMessage) {
        conversation_.push_back(
            {"智能体",
             QStringLiteral("我正在%1，等当前工具调用完成后再接管新任务。")
                 .arg(currentAgentAction_.isEmpty() ? "审计" : currentAgentAction_),
             "运行中"});
        emit sessionChanged();
        return;
    }

    auto cancellation = activeCancellation_;
    if (!cancellation) {
        cancellation = std::make_shared<std::atomic_bool>(false);
        activeCancellation_ = cancellation;
    }
    auto request = makeAgentRequest(message, context);
    request.isCancelled = [cancellation]() { return cancellation->load(); };
    if (request.projectRoot.empty() && result_ == nullptr) {
        runGeneralAssistant(message, context, appendUserMessage);
        return;
    }

    if (accessMode_ == "plan") {
        previewAgentPlan(message, context, appendUserMessage);
        return;
    }

    if (appendUserMessage) {
        conversation_.push_back({"用户", message, context});
    }

    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    status_ = brainReady ? "大模型审计助手正在分析" : "本地诊断：未启用大模型";
    emit statusChanged();

    if (brainReady) {
        cc::LlmConfig brainConfig;
        brainConfig.apiKey = llmApiKey_.toStdString();
        brainConfig.endpoint = llmEndpoint_.toStdString();
        brainConfig.model = llmModel_.toStdString();
        brainConfig.provider = llmProvider_.toStdString();
        brainConfig.apiKeyHeader = llmApiKeyHeader_.toStdString();
        brainConfig.apiKeyPrefix = llmApiKeyPrefix_.toStdString();
        brainConfig.allowNetwork = request.allowNetwork;
        brainConfig.allowLlm = request.allowLlm;
        brainConfig.isCancelled = [cancellation]() { return cancellation->load(); };

        auto requestCopy = request;
        auto auditSnapshot = result_;
        auto baselineSnapshot = baselineResult_;
        requestCopy.auditResult = auditSnapshot.get();
        requestCopy.baselineAuditResult = baselineSnapshot.get();
        const QPointer<CompileController> guard{this};
        const auto sessionId = activeSessionId_;
        brainWorkerRunning_ = true;
        auto* worker = QThread::create([guard, brainConfig = std::move(brainConfig),
                                        requestCopy = std::move(requestCopy), auditSnapshot,
                                        baselineSnapshot, sessionId, cancellation]() mutable {
            const auto streamEvent = [guard, sessionId, cancellation](const cc::AgentEvent& event) {
                if (cancellation->load()) {
                    return;
                }
                auto sharedEvent = std::make_shared<cc::AgentEvent>(event);
                if (guard.isNull()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    guard,
                    [guard, sharedEvent, sessionId, cancellation]() {
                        if (!guard.isNull() && !cancellation->load() &&
                            guard->activeSessionId_ == sessionId &&
                            guard->activeCancellation_ == cancellation) {
                            guard->appendAgentEvent(*sharedEvent);
                        }
                    },
                    Qt::QueuedConnection);
            };
            auto run = cc::BrainAgentLoop{}.run(brainConfig, requestCopy, streamEvent);
            if (run.ok() && run.value().auditResult.has_value()) {
                persistAuditPackage(run.value().auditResult.value());
            }
            auto outcome = std::make_shared<cc::Result<cc::AgentRunResult>>(std::move(run));
            if (guard.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                guard,
                [guard, outcome, sessionId, cancellation]() mutable {
                    if (guard.isNull() || cancellation->load() ||
                        guard->activeSessionId_ != sessionId ||
                        guard->activeCancellation_ != cancellation) {
                        return;
                    }
                    guard->brainWorkerRunning_ = false;
                    guard->applyAgentRunResult(std::move(*outcome), "LLM Brain", false);
                    guard->finishDeferredAgentConversation();
                },
                Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
        return;
    }

    conversation_.push_back({"系统", "未检测到已授权的大模型配置，本轮只执行本地上下文诊断。",
                             "Brain 未启用", "system", QString{}, true});
    auto requestCopy = request;
    auto auditSnapshot = result_;
    auto baselineSnapshot = baselineResult_;
    requestCopy.auditResult = auditSnapshot.get();
    requestCopy.baselineAuditResult = baselineSnapshot.get();
    const QPointer<CompileController> guard{this};
    const auto sessionId = activeSessionId_;
    brainWorkerRunning_ = true;
    auto* worker = QThread::create([guard, requestCopy = std::move(requestCopy), auditSnapshot,
                                    baselineSnapshot, sessionId, cancellation]() mutable {
        auto run = cc::AgentRuntime{}.runLocal(requestCopy);
        if (run.ok() && run.value().auditResult.has_value()) {
            persistAuditPackage(run.value().auditResult.value());
        }
        auto outcome = std::make_shared<cc::Result<cc::AgentRunResult>>(std::move(run));
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, outcome, sessionId, cancellation]() mutable {
                if (guard.isNull() || cancellation->load() ||
                    guard->activeSessionId_ != sessionId ||
                    guard->activeCancellation_ != cancellation) {
                    return;
                }
                guard->brainWorkerRunning_ = false;
                guard->applyAgentRunResult(std::move(*outcome), "本地诊断");
                guard->finishDeferredAgentConversation();
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::appendAgentEvent(const cc::AgentEvent& event) {
    workbench::SessionMessage message;
    message.ok =
        event.kind == cc::AgentEventKind::Tool ? event.payload.at("ok").asBool(false) : true;

    if (event.kind == cc::AgentEventKind::Plan) {
        const auto toolName = event.payload.at("call").at("name").asString();
        const auto label = friendlyToolName(toolName);
        message.role = "审计过程";
        message.kind = "tool";
        message.text = QStringLiteral("准备%1").arg(label);
        message.context = "审计流程";
        currentAgentAction_ = label;
        status_ = QStringLiteral("正在%1").arg(label);
    } else if (event.kind == cc::AgentEventKind::Tool) {
        const auto toolName = event.payload.at("tool_name").asString();
        const auto& output = event.payload.at("output");
        const auto title = output.at("title").asString();
        const auto detail = output.at("detail").asString();
        message.role = "审计工具";
        message.kind = "tool";
        message.text =
            title.empty() ? QString::fromStdString(event.text) : QString::fromStdString(title);
        message.detail = QString::fromStdString(detail);
        if (toolName == "run_project_audit") {
            message.text = "项目规则审计完成";
            message.detail = QString::fromStdString(output.at("summary").asString());
        }
        message.context = friendlyToolName(toolName);
        currentAgentAction_ = message.text;
        status_ = message.ok ? QStringLiteral("已%1").arg(message.text)
                             : QStringLiteral("%1未完成").arg(message.text);
    } else if (event.kind == cc::AgentEventKind::Assistant) {
        message.role = "项目审计助手";
        message.kind = "assistant";
        message.text = QString::fromStdString(event.text);
        message.context.clear();
        currentAgentAction_ = "整理审计结论";
    } else {
        message.role = "系统";
        message.kind = "system";
        message.text = QString::fromStdString(event.text);
        message.context.clear();
    }

    conversation_.push_back(std::move(message));
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::applyAgentRunResult(cc::Result<cc::AgentRunResult> run,
                                            const QString& planner, bool appendEvents) {
    if (!run.ok()) {
        status_ = friendlyFailure(run.error(), planner == "LLM Brain" ? "智能分析" : "当前操作");
        conversation_.push_back(
            {"系统",
             planner == "LLM Brain"
                 ? status_ + "\n你可以点击发送再次尝试；已经生成的本地规则检查结果仍然有效。"
                 : status_,
             "操作未完成", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    if (appendEvents) {
        for (const auto& event : run.value().events) {
            appendAgentEvent(event);
        }
    }
    agentResult_ = QString::fromStdString(run.value().finalAnswer);
    agentTrace_ = QString::fromStdString(cc::writeJson(cc::agentRunTraceJson(run.value()), 2));
    const bool producedAudit = run.value().auditResult.has_value();
    const bool producedDiff = run.value().auditDiff.has_value();
    if (producedDiff) {
        auditDiff_ = std::move(run.value().auditDiff.value());
    }
    if (producedAudit) {
        result_ = std::make_shared<cc::AuditResult>(std::move(run.value().auditResult.value()));
        if (!producedDiff || baselineResult_ == nullptr) {
            baselineResult_ = result_;
        }
        advisory_.reset();
        selectedFilePreview_.clear();
        compactedContext_.clear();
        if (!producedDiff) {
            auditDiff_.reset();
        }
        completedAuditSteps_ = static_cast<int>(auditStageCount());
        activeAuditStep_ = -1;
        currentAgentAction_ = producedDiff ? "修改后复查完成" : "项目文件检查完成";
    }
    status_ = producedAudit ? (producedDiff ? "修改后复查完成" : "项目文件检查完成")
              : planner == "LLM Brain" ? "大模型审计助手已完成分析"
                                       : "仅完成本地上下文诊断";
    emit statusChanged();
    if (producedAudit) {
        emit resultChanged();
        emit advisoryChanged();
        emit selectedFilePreviewChanged();
        emit agentStateChanged();
    }
    if (producedDiff) {
        emit auditDiffChanged();
    }
    emit agentResultChanged();
    emit agentTraceChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::startDeferredAgentConversation(const QString& message,
                                                       const QString& context,
                                                       bool appendUserMessage) {
    const auto trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (agentRunning_ || advisoryRunning_) {
        constexpr std::size_t kMaximumQueuedMessages = 8U;
        if (pendingComposerMessages_.size() >= kMaximumQueuedMessages) {
            status_ = "发送队列已满，请等待当前任务结束或停止任务";
            emit statusChanged();
            return;
        }
        pendingComposerMessages_.push_back({trimmed, context, accessMode_, appendUserMessage});
        status_ = QStringLiteral("已加入发送队列（%1）")
                      .arg(static_cast<int>(pendingComposerMessages_.size()));
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    if (appendUserMessage) {
        conversation_.push_back({"用户", trimmed, context, "user", QString{}, true});
    }

    agentRunning_ = true;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeCancellation_ = cancellation;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_ = "思考中";
    status_ = "Agent 正在思考";
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();

    QTimer::singleShot(40, this, [this, trimmed, context, cancellation]() {
        if (cancellation->load() || activeCancellation_ != cancellation || !agentRunning_) {
            return;
        }
        runAgentConversation(trimmed, context, false);
        if (!brainWorkerRunning_) {
            finishDeferredAgentConversation();
        }
    });
}

void CompileController::finishDeferredAgentConversation() {
    if (!brainWorkerRunning_) {
        agentRunning_ = false;
        activeCancellation_.reset();
        if (currentAgentAction_ == "思考中") {
            currentAgentAction_.clear();
        }
        emit agentStateChanged();
    }

    emit workspaceChanged();
    emit sessionChanged();

    if (!agentRunning_) {
        flushQueuedComposerMessage();
    }
}

void CompileController::cancelCurrentJob() {
    if (!agentRunning_ && !advisoryRunning_) {
        return;
    }
    if (activeCancellation_) {
        activeCancellation_->store(true);
        activeCancellation_.reset();
    }
    agentRunning_ = false;
    brainWorkerRunning_ = false;
    advisoryRunning_ = false;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    pendingComposerMessages_.clear();
    status_ = "已停止当前任务";
    conversation_.push_back({"系统", "已停止当前任务，后台返回的过期结果不会写入会话。", "任务停止",
                             "system", QString{}, true});
    emit statusChanged();
    emit agentStateChanged();
    emit advisoryChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::flushQueuedComposerMessage() {
    if (agentRunning_ || advisoryRunning_ || pendingComposerMessages_.empty()) {
        return;
    }

    const auto next = pendingComposerMessages_.front();
    pendingComposerMessages_.erase(pendingComposerMessages_.begin());
    setAccessMode(next.accessMode);
    startDeferredAgentConversation(next.message, next.context, next.appendUserMessage);
}

cc::AgentRunRequest CompileController::makeAgentRequest(const QString& goal,
                                                        const QString& context) const {
    cc::AgentRunRequest request;
    request.userGoal = goal.toStdString();
    request.auditResult = result_.get();
    request.baselineAuditResult = baselineResult_.get();
    request.auditOptions.rulesDir = resolveRulesDir(projectPath_);
    request.permissionMode = accessMode_.toStdString();
    const bool optimizeWorkflow = context == "/optimize";
    request.requireWorkspaceChanges = optimizeWorkflow;
    request.requireReaudit = optimizeWorkflow;
    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    request.allowNetwork = brainReady;
    request.allowLlm = brainReady;
    const bool bypass = accessMode_ == "bypass";
    request.allowWriteWorkspace = accessMode_ == "code" || bypass;
    request.allowReadExternal = bypass;
    request.allowModifyOriginal = false;
    request.allowExecuteCommand = false;
    if (result_ != nullptr) {
        const auto instructions =
            cc::ProjectMemory{}.loadInstructions(result_->context.workspaceRoot);
        if (instructions.ok()) {
            request.projectInstructions = instructions.value();
        }
    }
    if (!compactedContext_.isEmpty()) {
        request.conversationHistory.push_back(
            {.role = "system", .content = compactedContext_.toStdString()});
    }
    std::size_t historyEnd = conversation_.size();
    const auto goalText = goal.trimmed();
    for (std::size_t index = conversation_.size(); index > 0U; --index) {
        const auto& item = conversation_.at(index - 1U);
        if (item.kind.trimmed().toLower() == "user" && item.text.trimmed() == goalText) {
            historyEnd = index;
            break;
        }
    }
    const auto historyStart = historyEnd > 20U ? historyEnd - 20U : 0U;
    for (std::size_t index = historyStart; index < historyEnd; ++index) {
        const auto& item = conversation_.at(index);
        const auto kind = item.kind.trimmed().toLower();
        if (kind != "user" && kind != "assistant") {
            continue;
        }
        const auto text = item.text.trimmed();
        if (text.isEmpty()) {
            continue;
        }
        request.conversationHistory.push_back(
            {.role = kind.toStdString(), .content = text.toStdString()});
    }
    if (result_ != nullptr) {
        request.projectRoot = bypass ? result_->context.originalRoot : result_->context.inputRoot;
        request.workspaceRoot = result_->context.workspaceRoot / "agent";
        return request;
    }
    const auto normalized = normalizedInputPath(projectPath_).trimmed();
    if (!normalized.isEmpty()) {
        const std::filesystem::path selected{normalized.toStdString()};
        std::error_code ec;
        const auto workspaceBase =
            std::filesystem::is_regular_file(selected, ec) ? selected.parent_path() : selected;
        request.projectRoot = selected;
        request.workspaceRoot = workspaceBase / ".project-trust" / "agent-workspace";
        request.requireAudit = true;
    }
    return request;
}

void CompileController::exportMarkdown(const QString& outputPath) {
    exportReport(outputPath, false);
}

void CompileController::exportJson(const QString& outputPath) {
    exportReport(outputPath, true);
}

void CompileController::exportReport(const QString& outputPath, bool jsonFormat) {
    if (result_ == nullptr) {
        status_ = "请先运行审计";
        emit statusChanged();
        return;
    }
    if (agentRunning_ || advisoryRunning_) {
        status_ = "请等待当前任务结束后再导出报告";
        emit statusChanged();
        return;
    }
    const auto normalized = normalizedInputPath(outputPath).trimmed();
    if (normalized.isEmpty()) {
        status_ = "请选择报告导出路径";
        emit statusChanged();
        return;
    }

    agentRunning_ = true;
    currentAgentAction_ = jsonFormat ? "导出 JSON 审计包" : "导出 Markdown 报告";
    status_ = QStringLiteral("正在后台%1").arg(currentAgentAction_);
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    activeCancellation_ = cancellation;
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();

    auto auditSnapshot = result_;
    auto diffSnapshot = auditDiff_;
    const auto sessionId = activeSessionId_;
    const QPointer<CompileController> guard{this};
    auto* worker = QThread::create([guard, auditSnapshot, diffSnapshot = std::move(diffSnapshot),
                                    normalized, jsonFormat, sessionId, cancellation]() mutable {
        cc::Result<void> written =
            cancellation->load() ? cc::Result<void>::failure("报告导出已取消")
            : jsonFormat
                ? cc::JsonReporter{}.write(*auditSnapshot, normalized.toStdString(),
                                           diffSnapshot.has_value() ? &(*diffSnapshot) : nullptr)
                : cc::MarkdownReporter{}.write(*auditSnapshot, normalized.toStdString(),
                                               diffSnapshot.has_value() ? &(*diffSnapshot)
                                                                        : nullptr);
        auto outcome = std::make_shared<cc::Result<void>>(std::move(written));
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, outcome, normalized, jsonFormat, sessionId, cancellation]() mutable {
                if (guard.isNull() || cancellation->load() ||
                    guard->activeSessionId_ != sessionId ||
                    guard->activeCancellation_ != cancellation) {
                    return;
                }
                guard->activeCancellation_.reset();
                guard->agentRunning_ = false;
                guard->currentAgentAction_.clear();
                if (!outcome->ok()) {
                    guard->status_ = QString::fromStdString(outcome->error());
                } else {
                    const auto format = jsonFormat ? QStringLiteral("JSON 审计包")
                                                   : QStringLiteral("Markdown 报告");
                    guard->status_ = format + "已导出";
                    guard->conversation_.push_back(
                        {"产物", QStringLiteral("%1已导出到：%2").arg(format, normalized),
                         "报告已导出", "artifact", QString{}, true, "report"});
                }
                emit guard->statusChanged();
                emit guard->agentStateChanged();
                emit guard->workspaceChanged();
                emit guard->sessionChanged();
                guard->flushQueuedComposerMessage();
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompileController::submitMessage(const QString& message) {
    const auto directPath = normalizedInputPath(message.trimmed());
    if (!directPath.isEmpty()) {
        const QFileInfo info{directPath};
        if (info.exists() && (info.isDir() || info.isFile())) {
            selectProject(directPath);
            return;
        }
    }

    auto routed = cc::AgentCommandRouter{}.route(message.toStdString());
    if (!routed.ok()) {
        const auto trimmed = message.trimmed();
        if (!trimmed.isEmpty()) {
            conversation_.push_back({"用户", trimmed, "来自输入框"});
        }
        status_ = QString::fromStdString(routed.error());
        conversation_.push_back({"系统", status_, "命令解析"});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    const auto command = routed.value();
    switch (command.kind) {
    case cc::AgentCommandKind::Empty:
        return;
    case cc::AgentCommandKind::RunAudit:
        conversation_.push_back({"用户", QString::fromStdString(command.prompt),
                                 QString::fromStdString(command.context)});
        emit sessionChanged();
        if (llmApproved_ && !llmApiKey_.isEmpty() &&
            !normalizedInputPath(projectPath_).trimmed().isEmpty()) {
            result_.reset();
            baselineResult_.reset();
            auditDiff_.reset();
            advisory_.reset();
            agentResult_.clear();
            agentTrace_.clear();
            emit resultChanged();
            emit auditDiffChanged();
            emit advisoryChanged();
            emit agentResultChanged();
            emit agentTraceChanged();
            startDeferredAgentConversation(
                "请重新审查当前项目。必须先调用 run_project_audit，再依据规则和证据结果回答。",
                QString::fromStdString(command.context), false);
            return;
        }
        runAudit();
        return;
    case cc::AgentCommandKind::PlanTask: {
        const auto prompt = QString::fromStdString(command.prompt).trimmed();
        setAccessMode("plan");
        if (prompt.isEmpty()) {
            status_ = "已切换到计划模式";
            conversation_.push_back(
                {"系统", "已切换到计划模式。", "权限模式", "system", QString{}, true});
            emit statusChanged();
            emit sessionChanged();
            return;
        }
        previewAgentPlan(prompt, QString::fromStdString(command.context));
        return;
    }
    case cc::AgentCommandKind::SetPermissionMode: {
        conversation_.push_back(
            {"用户", message.trimmed(), QString::fromStdString(command.context)});
        const auto mode = QString::fromStdString(command.prompt).trimmed();
        if (mode.isEmpty()) {
            status_ = "当前权限模式：" + accessModeLabel();
            conversation_.push_back(
                {"系统", sessionStatusText(), "权限模式", "system", QString{}, true});
            emit statusChanged();
            emit sessionChanged();
            return;
        }
        setAccessMode(mode);
        status_ = "已切换到" + accessModeLabel();
        conversation_.push_back({"系统", status_, "权限模式", "system", QString{}, true});
        emit statusChanged();
        emit sessionChanged();
        return;
    }
    case cc::AgentCommandKind::ShowStatus:
        conversation_.push_back({"用户", QString::fromStdString(command.prompt),
                                 QString::fromStdString(command.context)});
        conversation_.push_back(
            {"系统", sessionStatusText(), "会话状态", "system", QString{}, true});
        status_ = "已显示会话状态";
        emit statusChanged();
        emit sessionChanged();
        return;
    case cc::AgentCommandKind::CompactContext:
        compactedContext_ = compactedContextText();
        {
            std::vector<workbench::SessionMessage> recent;
            constexpr std::size_t kRetainedConversationMessages = 6U;
            for (auto item = conversation_.rbegin();
                 item != conversation_.rend() && recent.size() < kRetainedConversationMessages;
                 ++item) {
                if (item->kind == "user" || item->kind == "assistant") {
                    recent.push_back(*item);
                }
            }
            std::reverse(recent.begin(), recent.end());
            conversation_.clear();
            conversation_.push_back(
                {"系统", compactedContext_, "上下文压缩", "system", QString{}, true});
            conversation_.insert(conversation_.end(), recent.begin(), recent.end());
        }
        status_ = "已压缩当前上下文";
        emit statusChanged();
        emit sessionChanged();
        return;
    case cc::AgentCommandKind::NewSession:
        newSession();
        return;
    case cc::AgentCommandKind::ShowHelp:
        conversation_.push_back({"用户", QString::fromStdString(command.prompt),
                                 QString::fromStdString(command.context)});
        conversation_.push_back(
            {"系统", QString::fromStdString(cc::agentCommandHelpText()), "命令帮助"});
        status_ = "已显示智能体命令帮助";
        emit statusChanged();
        emit sessionChanged();
        return;
    case cc::AgentCommandKind::RunModePrefixedTask: {
        const auto commandContext = QString::fromStdString(command.context);
        const auto mode = commandContext == "/optimize" ? QStringLiteral("code")
                                                        : QString{commandContext}.remove(0, 1);
        setAccessMode(mode);
        startDeferredAgentConversation(QString::fromStdString(command.prompt), commandContext,
                                       true);
        return;
    }
    case cc::AgentCommandKind::RunAgentTask:
        startDeferredAgentConversation(QString::fromStdString(command.prompt),
                                       QString::fromStdString(command.context), true);
        return;
    }
}
