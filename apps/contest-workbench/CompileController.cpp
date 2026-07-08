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

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>

#include <cstdlib>
#include <filesystem>
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
    const auto anthropicToken = envText("ANTHROPIC_AUTH_TOKEN");
    if (!anthropicToken.isEmpty()) {
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
    } else if (llmApiKey_.isEmpty() && llmApproved_) {
        llmApproved_ = false;
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
    setProjectPath(path);
    conversation_.push_back({"系统", QStringLiteral("已选择项目：%1").arg(path), accessModeLabel(),
                             "system", QString{}, true});
    status_ = "已选择项目，可开始审计或让智能体翻阅材料";
    emit statusChanged();
    emit sessionChanged();
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
             "生成补证任务和修复计划。整个过程只读取隔离副本，不覆盖原项目。")
             .arg(stringText(begun.value().sessionId)),
         "审计计划"});
    emit statusChanged();
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
        return;
    }
    result_ = std::move(result.value());
    (void)cc::ProjectMemory{}.init(result_->context.workspaceRoot, result_->cpir.competitionType);
    (void)cc::AuditSessionStore{}.save(*result_, result_->context.workspaceRoot / "audit.json");
    agentRunning_ = false;
    completedAuditSteps_ = static_cast<int>(auditStageCount());
    currentAgentAction_ = "审计完成";
    status_ = "审计完成";
    conversation_.push_back(
        {"工具", "已完成材料整理、证据匹配、规则检查和评分。",
         QStringLiteral("会话 %1").arg(stringText(result_->context.sessionId))});
    conversation_.push_back({"智能体", agentSummary(), "下一步建议"});
    emit statusChanged();
    emit agentStateChanged();
    emit resultChanged();
    emit workspaceChanged();
    emit sessionChanged();
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
    runAgentConversation(trimmed, "Brain 任务");
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
    const auto goal = message.trimmed().isEmpty()
                          ? QStringLiteral("为当前竞赛项目生成下一步审计计划。")
                          : message.trimmed();
    conversation_.push_back({"用户", goal, context});

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

void CompileController::runAgentConversation(const QString& message, const QString& context) {
    if (agentRunning_) {
        conversation_.push_back(
            {"智能体",
             QStringLiteral("我正在%1，等当前工具调用完成后再接管新任务。")
                 .arg(currentAgentAction_.isEmpty() ? "审计" : currentAgentAction_),
             "运行中"});
        emit sessionChanged();
        return;
    }

    if (accessMode_ == "plan") {
        previewAgentPlan(message, context);
        return;
    }

    conversation_.push_back({"用户", message, context});
    const auto request = makeAgentRequest(message);
    if (request.projectRoot.empty() && !result_.has_value()) {
        status_ = "请先选择项目材料包";
        conversation_.push_back(
            {"智能体", "先选择项目目录或材料包，我才能自动翻阅文件并执行受控工具。", "等待材料"});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    status_ = brainReady ? "Brain 正在运行工具循环" : "本地诊断：未启用 Brain";
    emit statusChanged();

    cc::Result<cc::AgentRunResult> run = cc::Result<cc::AgentRunResult>::failure("未执行");
    QString planner = brainReady ? "LLM Brain" : "本地诊断";
    cc::LlmConfig brainConfig;
    if (brainReady) {
        brainConfig.apiKey = llmApiKey_.toStdString();
        brainConfig.endpoint = llmEndpoint_.toStdString();
        brainConfig.model = llmModel_.toStdString();
        brainConfig.provider = llmProvider_.toStdString();
        brainConfig.apiKeyHeader = llmApiKeyHeader_.toStdString();
        brainConfig.apiKeyPrefix = llmApiKeyPrefix_.toStdString();
        brainConfig.allowNetwork = request.allowNetwork;
        brainConfig.allowLlm = request.allowLlm;
        auto brainRun = cc::BrainAgentLoop{}.run(brainConfig, request);
        if (brainRun.ok()) {
            run = std::move(brainRun);
            planner = "LLM Brain";
        } else {
            conversation_.push_back(
                {"Brain",
                 QStringLiteral("LLM 工具循环失败：%1\n下面只执行本地上下文诊断，不冒充模型回答。")
                     .arg(QString::fromStdString(brainRun.error())),
                 "Brain 失败", "system", QString{}, true});
            run = cc::AgentRuntime{}.runLocal(request);
            planner = "本地诊断";
        }
    } else {
        conversation_.push_back({"系统", "未检测到已授权的大模型配置，本轮只执行本地上下文诊断。",
                                 "Brain 未启用", "system", QString{}, true});
        run = cc::AgentRuntime{}.runLocal(request);
    }

    if (!run.ok()) {
        status_ = QString::fromStdString(run.error());
        conversation_.push_back({"系统", status_, "智能体失败"});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    for (const auto& event : run.value().events) {
        conversation_.push_back(
            {QString::fromStdString(event.role), QString::fromStdString(event.text),
             QString::fromStdString(event.context),
             QString::fromStdString(cc::toString(event.kind)), QString{}, true});
    }
    agentResult_ = QString::fromStdString(run.value().finalAnswer);
    agentTrace_ = QString::fromStdString(cc::writeJson(cc::agentRunTraceJson(run.value()), 2));
    status_ = planner == "LLM Brain" ? "LLM Brain 已完成受控工具调用" : "仅完成本地上下文诊断";
    emit statusChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

cc::AgentRunRequest CompileController::makeAgentRequest(const QString& goal) const {
    cc::AgentRunRequest request;
    request.userGoal = goal.toStdString();
    request.auditResult = result_.has_value() ? &(*result_) : nullptr;
    request.permissionMode = accessMode_.toStdString();
    const bool brainReady = llmApproved_ && !llmApiKey_.isEmpty();
    request.allowNetwork = brainReady;
    request.allowLlm = brainReady;
    // Bypass 模式：经用户明确授权后，智能体可读取原项目位置，并放开高风险能力标记。
    const bool bypass = accessMode_ == "bypass";
    request.allowReadExternal = bypass;
    request.allowModifyOriginal = bypass;
    request.allowExecuteCommand = bypass;
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
        runAgentConversation(QString::fromStdString(command.prompt),
                             QString::fromStdString(command.context));
        return;
    }
    case cc::AgentCommandKind::RunAgentTask:
        runAgentConversation(QString::fromStdString(command.prompt),
                             QString::fromStdString(command.context));
        return;
    }
}
