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
#include "cc/report/JsonReporter.hpp"
#include "cc/report/MarkdownReporter.hpp"
#include "cc/util/FileUtil.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
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

[[nodiscard]] QString envText(const char* name) {
    const auto* value = std::getenv(name);
    return value == nullptr ? QString{} : QString::fromUtf8(value).trimmed();
}

[[nodiscard]] QString maskedSecret() {
    return QStringLiteral("********");
}

[[nodiscard]] bool isMaskedSecret(const QString& value) {
    return value == maskedSecret();
}

[[nodiscard]] QString endpointFromBaseUrl(QString baseUrl, const QString& defaultPath) {
    baseUrl = baseUrl.trimmed();
    if (baseUrl.isEmpty()) {
        return {};
    }
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    if (baseUrl.endsWith("/chat/completions") || baseUrl.endsWith("/messages")) {
        return baseUrl;
    }
    return baseUrl + defaultPath;
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
        return "整理项目材料";
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
        return "阅读项目材料";
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
    return "执行受控审计步骤";
}

[[nodiscard]] bool wantsOptimization(const QString& message) {
    const auto text = message.trimmed().toLower();
    if (text.isEmpty()) {
        return false;
    }
    return text == "是" || text == "好的" || text == "可以" || text == "开始" ||
           text == "开始优化" || text == "优化" || text == "修复" || text == "yes" ||
           text == "ok" || text.contains("帮我优化") || text.contains("开始优化") ||
           text.contains("继续优化") || text.contains("修复项目") || text.contains("优化项目");
}

[[nodiscard]] QString defectReportText(const cc::AuditResult& result) {
    QStringList lines;
    lines << QStringLiteral("缺点评审完成：确定性评分 %1/100，可信债务 %2。")
                 .arg(result.trustScore.totalScore)
                 .arg(result.trustScore.trustDebt);
    lines << QStringLiteral("我只列影响交付可信度的问题，不写无关优点。");

    if (result.findings.empty() && result.consistencyIssues.empty() && result.fixTasks.empty()) {
        lines << "当前规则包没有抓到必须处理项或补证任务。建议仍做人工复核后再提交。";
        return lines.join("\n");
    }

    if (!result.findings.empty()) {
        lines << "\n规则命中的缺点：";
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
            lines << QStringLiteral("%1. [%2] %3：%4")
                         .arg(static_cast<int>(index + 1U))
                         .arg(stringText(task.priority))
                         .arg(stringText(task.title))
                         .arg(stringText(task.reason));
        }
    }

    lines << "\n是否需要我继续优化项目？回复“是”或“开始优化”，我会把缺点修复方案和补证清单写到安全"
             "工作区。";
    return lines.join("\n");
}

[[nodiscard]] std::string optimizationMarkdown(const cc::AuditResult& result) {
    std::ostringstream output;
    output << "# 项目缺点优化方案\n\n";
    output << "本文件由 Workbench 根据规则审计、证据匹配和一致性检查生成。它只处理缺点，"
              "不伪造用户、营收、合作、专利、实验结果或市场数据，也不覆盖原项目。\n\n";
    output << "## 当前评分\n\n";
    output << "- 可信评分：" << result.trustScore.totalScore << "/100\n";
    output << "- 可信债务：" << result.trustScore.trustDebt << "\n";
    output << "- 规则问题：" << result.findings.size() << "\n";
    output << "- 补证任务：" << result.fixTasks.size() << "\n\n";

    output << "## 必须处理的缺点\n\n";
    bool hasBlocker = false;
    for (const auto& finding : result.findings) {
        if (finding.severity != cc::Severity::Blocker) {
            continue;
        }
        hasBlocker = true;
        output << "- [" << finding.ruleId << "] " << finding.title << "：" << finding.reason
               << "\n  - 修复建议：" << finding.fixSuggestion << "\n";
    }
    if (!hasBlocker) {
        output << "暂无 blocker 级问题。\n";
    }

    output << "\n## 补证清单\n\n";
    if (result.fixTasks.empty()) {
        output << "暂无补证任务。\n";
    }
    for (const auto& task : result.fixTasks) {
        output << "- [ ] " << task.title << "\n";
        output << "  - 优先级：" << task.priority << "\n";
        output << "  - 原因：" << task.reason << "\n";
        if (!task.requiredMaterial.empty()) {
            output << "  - 需要材料：";
            for (std::size_t index = 0U; index < task.requiredMaterial.size(); ++index) {
                output << (index == 0U ? "" : "、") << task.requiredMaterial.at(index);
            }
            output << "\n";
        }
    }

    output << "\n## 修复计划\n\n";
    output << result.repairPlan.markdown << "\n";
    if (!result.repairPlan.diffText.empty()) {
        output << "\n## Diff-first 草稿\n\n";
        output << "```diff\n" << result.repairPlan.diffText << "\n```\n";
    }
    return output.str();
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

} // namespace

CompileController::CompileController(QObject* parent) : QObject(parent) {
    const auto deepSeekKey = envText("DEEPSEEK_API_KEY").isEmpty() ? envText("DEEPSEEK_AUTH_TOKEN")
                                                                   : envText("DEEPSEEK_API_KEY");
    if (!deepSeekKey.isEmpty()) {
        llmApiKey_ = deepSeekKey;
        llmProvider_ = "deepseek";
        llmApiKeyHeader_ = "Authorization";
        llmApiKeyPrefix_ = "Bearer ";
        const auto endpoint =
            endpointFromBaseUrl(envText("DEEPSEEK_BASE_URL"), "/chat/completions");
        if (!endpoint.isEmpty()) {
            llmEndpoint_ = endpoint;
        }
        const auto model = envText("DEEPSEEK_MODEL");
        if (!model.isEmpty()) {
            llmModel_ = model;
        }
    }

    const auto anthropicToken = envText("ANTHROPIC_AUTH_TOKEN");
    if (llmApiKey_.isEmpty() && !anthropicToken.isEmpty()) {
        llmApiKey_ = anthropicToken;
        llmProvider_ = "anthropic";
        llmApiKeyHeader_ = "Authorization";
        llmApiKeyPrefix_ = "Bearer ";
        const auto endpoint = endpointFromBaseUrl(envText("ANTHROPIC_BASE_URL"), "/v1/messages");
        if (!endpoint.isEmpty()) {
            llmEndpoint_ = endpoint;
        }
        const auto model = envText("ANTHROPIC_MODEL");
        if (!model.isEmpty()) {
            llmModel_ = model;
        }
    }

    const auto openAiKey = envText("OPENAI_API_KEY");
    if (llmApiKey_.isEmpty() && !openAiKey.isEmpty()) {
        llmApiKey_ = openAiKey;
        llmProvider_ = "openai";
        llmApiKeyHeader_ = "Authorization";
        llmApiKeyPrefix_ = "Bearer ";
        const auto endpoint =
            endpointFromBaseUrl(envText("OPENAI_BASE_URL"), "/v1/chat/completions");
        if (!endpoint.isEmpty()) {
            llmEndpoint_ = endpoint;
        }
        const auto model = envText("OPENAI_MODEL");
        if (!model.isEmpty()) {
            llmModel_ = model;
        }
    }

    if (!llmApiKey_.isEmpty()) {
        llmApproved_ = true;
    }

    auditTimer_.setInterval(240);
    connect(&auditTimer_, &QTimer::timeout, this, [this]() { advanceAuditRun(); });
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

int CompileController::trustScore() const {
    return result_.has_value() ? result_->trustScore.totalScore : 0;
}

int CompileController::blockerCount() const {
    return result_.has_value() ? workbench::blockerCount(*result_) : 0;
}

int CompileController::warningCount() const {
    return result_.has_value() ? workbench::warningCount(*result_) : 0;
}

QString CompileController::summary() const {
    if (!result_.has_value()) {
        return "尚未运行审计";
    }
    return workbench::summary(*result_);
}

QVariantList CompileController::assets() const {
    return result_.has_value() ? workbench::assets(*result_) : QVariantList{};
}

QVariantList CompileController::roleDistribution() const {
    return result_.has_value() ? workbench::roleDistribution(*result_) : QVariantList{};
}

QVariantMap CompileController::cpir() const {
    return result_.has_value() ? workbench::cpir(*result_) : QVariantMap{};
}

QVariantList CompileController::claimEvidence() const {
    return result_.has_value() ? workbench::claimEvidence(*result_) : QVariantList{};
}

QVariantList CompileController::consistencyIssues() const {
    return result_.has_value() ? workbench::consistencyIssues(*result_) : QVariantList{};
}

QVariantList CompileController::findings() const {
    return result_.has_value() ? workbench::findings(*result_) : QVariantList{};
}

QVariantList CompileController::fixTasks() const {
    return result_.has_value() ? workbench::fixTasks(*result_) : QVariantList{};
}

QVariantList CompileController::scorePenalties() const {
    return result_.has_value() ? workbench::scorePenalties(*result_) : QVariantList{};
}

QVariantMap CompileController::projectContext() const {
    return workbench::projectContext(result_, normalizedInputPath(projectPath_));
}

QVariantList CompileController::sessionHistory() const {
    return workbench::sessionHistory(result_, conversation_, normalizedInputPath(projectPath_));
}

QVariantList CompileController::toolCards() const {
    return workbench::toolCards(result_, auditDiff_, agentRunning_, activeAuditStep_,
                                completedAuditSteps_);
}

QVariantList CompileController::permissionCards() const {
    return workbench::permissionCards(llmApproved_, accessMode_);
}

QVariantList CompileController::artifacts() const {
    return workbench::artifacts(result_, auditDiff_, agentResult_);
}

QString CompileController::agentSummary() const {
    return workbench::agentSummary(result_);
}

bool CompileController::agentRunning() const {
    return agentRunning_;
}

int CompileController::agentProgress() const {
    const auto total = static_cast<int>(auditStageCount()) + 1;
    if (agentRunning_) {
        if (activeAuditStep_ < 0) {
            return 0;
        }
        return std::clamp((completedAuditSteps_ * 100) / total, 0, 99);
    }
    return result_.has_value() ? 100 : 0;
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
    if (!llmApiKey_.isEmpty() && !llmApproved_) {
        llmApproved_ = true;
        emit workspaceChanged();
    }
    emit llmConfigChanged();
}

QString CompileController::llmEndpoint() const {
    return llmEndpoint_;
}

void CompileController::setLlmEndpoint(const QString& value) {
    if (llmEndpoint_ == value) {
        return;
    }
    llmEndpoint_ = value;
    emit llmConfigChanged();
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
    if (llmApproved_ == value) {
        return;
    }
    llmApproved_ = value;
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
        key == "direct" || key == "full" || key == "full-access") {
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

void CompileController::selectProject(const QString& urlOrPath) {
    const auto path = normalizedInputPath(urlOrPath).trimmed();
    if (path.isEmpty()) {
        return;
    }
    if (agentRunning_) {
        conversation_.push_back({"系统", "当前审计仍在运行，完成后再切换新项目。", "项目导入",
                                 "system", QString{}, false});
        emit sessionChanged();
        return;
    }
    setProjectPath(path);
    result_.reset();
    auditDiff_.reset();
    advisory_.reset();
    agentResult_.clear();
    agentTrace_.clear();
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    conversation_.push_back(
        {"用户", QStringLiteral("审计项目：%1").arg(path), "项目导入", "user", QString{}, true});
    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    conversation_.push_back(
        {"智能体",
         brainReady
             ? "已接收项目。大模型审计助手会先运行材料与规则审计，再结合证据结果阅读项目"
               "内容并给出清楚的改进建议。"
             : "已接收项目。当前未配置 LLM Brain，将运行本地确定性规则审计；评分仍由"
               "规则和证据链生成。",
         brainReady ? "智能审计" : "本地规则审计", "assistant", QString{}, true});
    status_ = brainReady ? "大模型审计助手正在分析项目" : "正在启动本地规则审计";
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit advisoryChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit workspaceChanged();
    emit sessionChanged();
    if (brainReady) {
        startDeferredAgentConversation(
            "请审查当前项目。先调用 run_project_audit 获取确定性规则、证据匹配和评分结果，"
            "再根据审计观察判断是否需要继续读取材料，最后回答主要缺点和下一步。",
            "项目导入", false);
        return;
    }
    runAudit();
}

void CompileController::newSession() {
    if (agentRunning_) {
        return;
    }
    auditRun_.reset();
    conversation_.clear();
    result_.reset();
    auditDiff_.reset();
    agentResult_.clear();
    agentTrace_.clear();
    projectPath_.clear();
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    status_ = "已开始新会话，选择竞赛项目后即可审计";
    emit projectPathChanged();
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

QVariantList CompileController::sessionList() const {
    QVariantList items;
    const auto hasProject = !normalizedInputPath(projectPath_).trimmed().isEmpty();
    if (!hasProject && !result_.has_value()) {
        return items;
    }
    QVariantMap current;
    if (result_.has_value()) {
        current["title"] = QString::fromStdString(result_->context.projectName.empty()
                                                      ? result_->context.sessionId
                                                      : result_->context.projectName);
        current["subtitle"] = QStringLiteral("评分 %1 · 必须处理 %2")
                                  .arg(result_->trustScore.totalScore)
                                  .arg(workbench::blockerCount(*result_));
    } else {
        const auto path = normalizedInputPath(projectPath_).trimmed();
        current["title"] = QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName();
        current["subtitle"] = agentRunning_ ? "审计进行中" : "待审计";
    }
    current["active"] = true;
    items.push_back(current);
    return items;
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

void CompileController::runAdvisory() {
    if (advisoryRunning_ || agentRunning_) {
        return;
    }
    if (!result_.has_value()) {
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

    auto proposed = cc::LlmBrain{}.requestAuditAdvisory(config, *result_);
    if (!proposed.ok()) {
        advisoryRunning_ = false;
        status_ = QString::fromStdString(proposed.error());
        conversation_.push_back({"系统", status_, "研判失败", "system", QString{}, false});
        emit statusChanged();
        emit advisoryChanged();
        emit sessionChanged();
        return;
    }

    advisory_ = cc::AdvisoryReconciler{}.reconcile(proposed.value(), *result_);
    advisoryRunning_ = false;
    status_ = "混合研判完成";
    conversation_.push_back({"智能体", QString::fromStdString(advisory_->summary), "混合研判",
                             "assistant", QString{}, advisory_->conflictingCount == 0});
    for (const auto& item : advisory_->items) {
        const bool ok = item.verdict != cc::AdvisoryVerdict::Conflicting;
        conversation_.push_back({"工具", QString::fromStdString(item.advisory.title),
                                 QString::fromStdString(cc::toString(item.verdict)), "tool",
                                 QString::fromStdString(item.reconciliation), ok});
    }
    emit statusChanged();
    emit advisoryChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::runInlineAdvisory() {
    if (!result_.has_value()) {
        return;
    }
    if (!llmApproved_ || llmApiKey_.isEmpty()) {
        conversation_.push_back(
            {"系统", "未配置 LLM Brain，本轮使用确定性规则、证据匹配和一致性检查完成评审。",
             "混合研判", "system", QString{}, true});
        return;
    }

    advisoryRunning_ = true;
    emit advisoryChanged();

    cc::LlmConfig config;
    config.apiKey = llmApiKey_.toStdString();
    config.endpoint = llmEndpoint_.toStdString();
    config.model = llmModel_.toStdString();
    config.provider = llmProvider_.toStdString();
    config.apiKeyHeader = llmApiKeyHeader_.toStdString();
    config.apiKeyPrefix = llmApiKeyPrefix_.toStdString();
    config.allowNetwork = true;
    config.allowLlm = true;

    auto proposed = cc::LlmBrain{}.requestAuditAdvisory(config, *result_);
    if (!proposed.ok()) {
        advisoryRunning_ = false;
        conversation_.push_back({"系统",
                                 QStringLiteral("LLM Brain 研判失败，已回退到确定性规则结果：%1")
                                     .arg(QString::fromStdString(proposed.error())),
                                 "混合研判", "system", QString{}, false});
        emit advisoryChanged();
        return;
    }

    advisory_ = cc::AdvisoryReconciler{}.reconcile(proposed.value(), *result_);
    advisoryRunning_ = false;
    conversation_.push_back({"工具", "LLM Brain 的风险判断已与规则和证据逐条校验。",
                             QStringLiteral("确认 %1 · 待核实 %2 · 冲突 %3")
                                 .arg(static_cast<int>(advisory_->confirmedCount))
                                 .arg(static_cast<int>(advisory_->unverifiedCount))
                                 .arg(static_cast<int>(advisory_->conflictingCount)),
                             "tool", QString::fromStdString(advisory_->summary),
                             advisory_->conflictingCount == 0});
    emit advisoryChanged();
}

void CompileController::runGeneralAssistant(const QString& message, const QString& context) {
    runGeneralAssistant(message, context, true);
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

    std::vector<cc::LlmMessage> messages{
        {.role = "system",
         .content = "你是竞赛项目可信评审平台中的常规问答助手。回答要清楚、直接；涉及项目评审时"
                    "提醒用户拖入项目以便结合规则和证据，不要伪造材料或评分。"}};
    const auto historyStart =
        conversation_.size() > 12U ? conversation_.size() - 12U : 0U;
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
    auto response = cc::LlmBrain{}.complete(config, messages);
    if (!response.ok()) {
        status_ = QString::fromStdString(response.error());
        conversation_.push_back({"系统", status_, "常规问答", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    status_ = "常规问答已完成";
    conversation_.push_back({"智能体", QString::fromStdString(response.value().content), "常规问答",
                             "assistant", QString{}, true});
    emit statusChanged();
    emit sessionChanged();
}

void CompileController::runOptimization() {
    if (!result_.has_value()) {
        status_ = "请先完成项目评审，再开始优化";
        conversation_.push_back({"系统", status_, "优化项目", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    const auto output =
        result_->context.workspaceRoot / "agent" / "optimization" / "defect-fix-plan.md";
    auto written = cc::util::writeTextFile(output, optimizationMarkdown(*result_));
    if (!written.ok()) {
        status_ = QString::fromStdString(written.error());
        conversation_.push_back({"系统", status_, "优化项目", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    agentResult_ = QStringLiteral("已生成缺点优化方案：%1")
                       .arg(QString::fromStdString(cc::util::pathString(output)));
    status_ = "优化方案已写入安全工作区";
    conversation_.push_back({"产物", agentResult_, "优化产物", "artifact", QString{}, true});
    conversation_.push_back(
        {"智能体",
         "我已把缺点修复方案、补证清单和 diff-first 草稿写入工作区。默认不会覆盖原项目；"
         "完成补证或手动采纳修订后，可以再次拖入优化后的项目做二次审计。",
         "优化完成", "assistant", QString{}, true});
    emit statusChanged();
    emit agentResultChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::runAudit() {
    if (agentRunning_) {
        status_ = "审计正在进行";
        emit statusChanged();
        return;
    }
    if (normalizedInputPath(projectPath_).trimmed().isEmpty()) {
        status_ = "请先选择项目材料包";
        conversation_.push_back(
            {"智能体", "先把项目目录或压缩包拖进来，我再开始审计。", "等待材料"});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    cc::AuditOptions options;
    options.rulesDir = resolveRulesDir(projectPath_);
    const auto normalizedPath = normalizedInputPath(projectPath_);

    result_.reset();
    auditDiff_.reset();
    advisory_.reset();
    agentResult_.clear();
    agentTrace_.clear();
    auditRun_ = std::make_unique<cc::StagedAuditPipeline>();
    auto begun = auditRun_->begin(normalizedPath.toStdString(), options);
    if (!begun.ok()) {
        auditRun_.reset();
        status_ = QString::fromStdString(begun.error());
        conversation_.push_back({"系统", status_, "审计失败", "system", QString{}, false});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    agentRunning_ = true;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_ = "制定审计计划";
    status_ = "正在制定审计计划";
    conversation_.push_back(
        {"计划",
         QStringLiteral(
             "已建立安全工作副本（会话 %1）。我会按受控流程逐步执行：整理材料、读取文本、"
             "判断赛道、生成项目画像、抽取声明、匹配证据、检查一致性、执行规则、计算评分、"
             "生成补证任务和修复计划。评审输出只抓缺点，最终评分由规则与证据裁决；整个过程"
             "只读取隔离副本，不覆盖原项目。")
             .arg(stringText(begun.value().sessionId)),
         "审计计划"});
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit advisoryChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
    auditTimer_.start();
}

void CompileController::advanceAuditRun() {
    if (auditRun_ == nullptr) {
        auditTimer_.stop();
        return;
    }

    if (auditRun_->hasNext()) {
        auto observed = auditRun_->advance();
        if (!observed.ok()) {
            auditTimer_.stop();
            auditRun_.reset();
            agentRunning_ = false;
            activeAuditStep_ = -1;
            currentAgentAction_ = "审计失败";
            status_ = QString::fromStdString(observed.error());
            conversation_.push_back({"系统", status_, "审计失败", "system", QString{}, false});
            emit statusChanged();
            emit agentStateChanged();
            emit workspaceChanged();
            emit sessionChanged();
            flushQueuedComposerMessage();
            return;
        }

        const auto& observation = observed.value();
        const auto title = stringText(observation.output.at("title").asString());
        const auto detail = stringText(observation.output.at("detail").asString());
        activeAuditStep_ = static_cast<int>(auditRun_->completedStages()) - 1;
        completedAuditSteps_ = static_cast<int>(auditRun_->completedStages());
        currentAgentAction_ = title;
        status_ = QStringLiteral("已%1").arg(title);
        conversation_.push_back({"工具", title, title, "tool", detail, observation.ok});
        emit statusChanged();
        emit agentStateChanged();
        emit workspaceChanged();
        emit sessionChanged();
        return;
    }

    auditTimer_.stop();
    completedAuditSteps_ = static_cast<int>(auditStageCount());
    activeAuditStep_ = -1;
    currentAgentAction_ = "汇总审计结果";
    status_ = "正在汇总审计结果";
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    finishAuditRun();
}

void CompileController::finishAuditRun() {
    if (auditRun_ == nullptr) {
        agentRunning_ = false;
        flushQueuedComposerMessage();
        return;
    }
    auto result = auditRun_->finish();
    auditRun_.reset();
    if (!result.ok()) {
        agentRunning_ = false;
        currentAgentAction_ = "审计失败";
        status_ = QString::fromStdString(result.error());
        conversation_.push_back({"系统", status_, "审计失败", "system", QString{}, false});
        emit statusChanged();
        emit agentStateChanged();
        emit workspaceChanged();
        emit sessionChanged();
        flushQueuedComposerMessage();
        return;
    }
    result_ = std::move(result.value());
    (void)cc::ProjectMemory{}.init(result_->context.workspaceRoot, result_->cpir.competitionType);
    (void)cc::AuditSessionStore{}.save(*result_, result_->context.workspaceRoot / "audit.json");
    agentRunning_ = false;
    completedAuditSteps_ = static_cast<int>(auditStageCount());
    currentAgentAction_ = "审计完成";
    status_ = "缺点评审完成，等待是否优化";
    conversation_.push_back(
        {"工具", "已完成材料整理、证据匹配、规则检查和评分。",
         QStringLiteral("会话 %1").arg(stringText(result_->context.sessionId))});
    runInlineAdvisory();
    conversation_.push_back(
        {"智能体", defectReportText(*result_), "缺点评审报告", "assistant", QString{}, true});
    emit statusChanged();
    emit agentStateChanged();
    emit resultChanged();
    emit workspaceChanged();
    emit sessionChanged();
    flushQueuedComposerMessage();
}

void CompileController::runDiff() {
    if (oldAuditPath_.isEmpty() || newAuditPath_.isEmpty()) {
        status_ = "请先填写两份 audit.json 路径";
        emit statusChanged();
        return;
    }
    auto diff = cc::DiffVerifier{}.diffFiles(normalizedInputPath(oldAuditPath_).toStdString(),
                                             normalizedInputPath(newAuditPath_).toStdString());
    if (!diff.ok()) {
        status_ = QString::fromStdString(diff.error());
        emit statusChanged();
        return;
    }
    auditDiff_ = diff.value();
    status_ = "二次审计差分完成";
    conversation_.push_back({"工具", "已基于两份审计数据包生成二次审计差分。", "差分完成"});
    emit statusChanged();
    emit auditDiffChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::runBrainTask(const QString& goal) {
    const auto trimmed = goal.trimmed().isEmpty()
                             ? QStringLiteral("请接管当前竞赛项目，翻阅材料并给出可信审计下一步。")
                             : goal.trimmed();
    startDeferredAgentConversation(trimmed, "Brain 任务", true);
}

QString CompileController::accessModeLabel() const {
    if (accessMode_ == "bypass") {
        return "Bypass 模式";
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
    lines << QStringLiteral("项目材料：%1").arg(normalized.isEmpty() ? "未选择" : normalized);
    lines << QStringLiteral("LLM Brain：%1")
                 .arg(llmApproved_ && !llmApiKey_.isEmpty() ? "已授权" : "本地受控");
    lines << QStringLiteral("当前状态：%1").arg(status_);
    if (result_.has_value()) {
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
    if (!result_.has_value()) {
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

    QStringList steps;
    steps << QStringLiteral("目标：%1").arg(goal);
    steps << "1. 固定读取当前项目状态、权限模式和已有审计结果。";
    if (!result_.has_value()) {
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

    if (result_.has_value() && wantsOptimization(message)) {
        if (appendUserMessage) {
            conversation_.push_back({"用户", message.trimmed(), context, "user", QString{}, true});
        }
        runOptimization();
        return;
    }

    const auto request = makeAgentRequest(message);
    if (request.projectRoot.empty() && !result_.has_value()) {
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

        auto requestCopy = request;
        auto auditSnapshot = std::make_shared<std::optional<cc::AuditResult>>();
        if (request.auditResult != nullptr) {
            *auditSnapshot = *request.auditResult;
            requestCopy.auditResult = &auditSnapshot->value();
        }
        const QPointer<CompileController> guard{this};
        brainWorkerRunning_ = true;
        auto* worker = QThread::create(
            [guard, brainConfig = std::move(brainConfig),
             requestCopy = std::move(requestCopy), auditSnapshot]() mutable {
                const auto streamEvent = [guard](const cc::AgentEvent& event) {
                    auto sharedEvent = std::make_shared<cc::AgentEvent>(event);
                    if (guard.isNull()) {
                        return;
                    }
                    QMetaObject::invokeMethod(
                        guard,
                        [guard, sharedEvent]() {
                            if (!guard.isNull()) {
                                guard->appendAgentEvent(*sharedEvent);
                            }
                        },
                        Qt::QueuedConnection);
                };
                auto outcome =
                    std::make_shared<cc::Result<cc::AgentRunResult>>(
                        cc::BrainAgentLoop{}.run(brainConfig, requestCopy, streamEvent));
                if (guard.isNull()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    guard,
                    [guard, outcome]() mutable {
                        if (guard.isNull()) {
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
    applyAgentRunResult(cc::AgentRuntime{}.runLocal(request), "本地诊断");
}

void CompileController::appendAgentEvent(const cc::AgentEvent& event) {
    workbench::SessionMessage message;
    message.ok = event.kind == cc::AgentEventKind::Tool
                     ? event.payload.at("ok").asBool(false)
                     : true;

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
        message.text = title.empty() ? QString::fromStdString(event.text)
                                     : QString::fromStdString(title);
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
        status_ = planner == "LLM Brain"
                      ? QStringLiteral("LLM 工具循环失败：%1").arg(
                            QString::fromStdString(run.error()))
                      : QString::fromStdString(run.error());
        conversation_.push_back(
            {"系统",
             planner == "LLM Brain"
                 ? status_ + "\n本轮已停止，没有静默切换成本地回答；请检查配置或网络后重试。"
                 : status_,
             "智能体失败", "system", QString{}, false});
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
    if (producedAudit) {
        result_ = std::move(run.value().auditResult.value());
        (void)cc::ProjectMemory{}.init(result_->context.workspaceRoot,
                                       result_->cpir.competitionType);
        (void)cc::AuditSessionStore{}.save(*result_,
                                           result_->context.workspaceRoot / "audit.json");
        completedAuditSteps_ = static_cast<int>(auditStageCount());
        activeAuditStep_ = -1;
        currentAgentAction_ = "项目材料审计完成";
    }
    status_ = producedAudit
                  ? "项目材料审计完成"
                  : planner == "LLM Brain" ? "大模型审计助手已完成分析"
                                           : "仅完成本地上下文诊断";
    emit statusChanged();
    if (producedAudit) {
        emit resultChanged();
        emit advisoryChanged();
        emit agentStateChanged();
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

    if (appendUserMessage) {
        conversation_.push_back({"用户", trimmed, context, "user", QString{}, true});
    }

    if (agentRunning_) {
        pendingComposerMessages_.push_back({trimmed, context});
        status_ = QStringLiteral("已加入发送队列（%1）")
                      .arg(static_cast<int>(pendingComposerMessages_.size()));
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    agentRunning_ = true;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_ = "思考中";
    status_ = "Agent 正在思考";
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();

    // Give QML a frame to paint the submitted turn and thinking indicator first.
    QTimer::singleShot(40, this, [this, trimmed, context]() {
        runAgentConversation(trimmed, context, false);
        if (!brainWorkerRunning_) {
            finishDeferredAgentConversation();
        }
    });
}

void CompileController::finishDeferredAgentConversation() {
    if (!auditTimer_.isActive()) {
        agentRunning_ = false;
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

void CompileController::flushQueuedComposerMessage() {
    if (agentRunning_ || pendingComposerMessages_.empty()) {
        return;
    }

    const auto next = pendingComposerMessages_.front();
    pendingComposerMessages_.erase(pendingComposerMessages_.begin());
    startDeferredAgentConversation(next.message, next.context, false);
}

cc::AgentRunRequest CompileController::makeAgentRequest(const QString& goal) const {
    cc::AgentRunRequest request;
    request.userGoal = goal.toStdString();
    request.auditResult = result_.has_value() ? &(*result_) : nullptr;
    request.auditOptions.rulesDir = resolveRulesDir(projectPath_);
    request.permissionMode = accessMode_.toStdString();
    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    request.allowNetwork = llmApproved_;
    request.allowLlm = brainReady;
    // Bypass 模式：经用户明确授权后，智能体可读取原项目位置，并放开高风险能力标记。
    const bool bypass = accessMode_ == "bypass";
    request.allowReadExternal = bypass;
    request.allowModifyOriginal = bypass;
    request.allowExecuteCommand = bypass;
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
    if (result_.has_value()) {
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
    if (!result_.has_value()) {
        status_ = "请先运行审计";
        emit statusChanged();
        return;
    }
    auto written = cc::MarkdownReporter{}.write(*result_, outputPath.toStdString());
    status_ = written.ok() ? "Markdown 报告已导出" : QString::fromStdString(written.error());
    if (written.ok()) {
        conversation_.push_back(
            {"产物", QStringLiteral("Markdown 报告已导出到：%1").arg(outputPath), "报告已导出"});
        emit workspaceChanged();
        emit sessionChanged();
    }
    emit statusChanged();
}

void CompileController::exportJson(const QString& outputPath) {
    if (!result_.has_value()) {
        status_ = "请先运行审计";
        emit statusChanged();
        return;
    }
    auto written = cc::JsonReporter{}.write(*result_, outputPath.toStdString());
    status_ = written.ok() ? "JSON 审计包已导出" : QString::fromStdString(written.error());
    if (written.ok()) {
        conversation_.push_back(
            {"产物", QStringLiteral("JSON 审计包已导出到：%1").arg(outputPath), "数据包已导出"});
        emit workspaceChanged();
        emit sessionChanged();
    }
    emit statusChanged();
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
        conversation_.push_back({"用户", QString::fromStdString(command.prompt),
                                 QString::fromStdString(command.context)});
        conversation_.push_back(
            {"系统", compactedContextText(), "上下文压缩", "system", QString{}, true});
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
        const auto mode = QString::fromStdString(command.context).remove(0, 1);
        setAccessMode(mode);
        startDeferredAgentConversation(QString::fromStdString(command.prompt),
                                       QString::fromStdString(command.context), true);
        return;
    }
    case cc::AgentCommandKind::RunAgentTask:
        startDeferredAgentConversation(QString::fromStdString(command.prompt),
                                       QString::fromStdString(command.context), true);
        return;
    }
}
