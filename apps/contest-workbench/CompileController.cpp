/**
 * @file CompileController.cpp
 * @brief QML 与 C++ Core 的桥接控制器实现。
 */

#include "CompileController.hpp"

#include "AuditResultModels.hpp"
#include "WorkbenchSessionModels.hpp"
#include "cc/agent/AuditPipeline.hpp"
#include "cc/agent/AuditSessionStore.hpp"
#include "cc/agent/ProjectMemory.hpp"
#include "cc/audit/DiffVerifier.hpp"
#include "cc/llm/LlmBrain.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/report/MarkdownReporter.hpp"

#include <QCoreApplication>
#include <QUrl>

#include <array>
#include <filesystem>
#include <QStringList>
#include <vector>

namespace {

[[nodiscard]] QString normalizedInputPath(const QString& value) {
    const QUrl url{value};
    return url.isLocalFile() ? url.toLocalFile() : value;
}

[[nodiscard]] QString stringText(const std::string& value) {
    return QString::fromStdString(value);
}

[[nodiscard]] const std::vector<std::string>& auditToolFlow() {
    static const std::vector<std::string> flow = {
        "inventory_project",       "extract_text",       "detect_competition_type",
        "build_cpir",              "extract_claims",     "match_evidence",
        "check_consistency",       "run_rules",          "calculate_trust_score",
        "generate_fix_tasks",      "generate_repair_plan"};
    return flow;
}

[[nodiscard]] QString toolLabel(const std::string& name) {
    if (name == "inventory_project") {
        return "整理材料";
    }
    if (name == "extract_text") {
        return "读取文本";
    }
    if (name == "detect_competition_type") {
        return "判断赛道";
    }
    if (name == "build_cpir") {
        return "生成项目画像";
    }
    if (name == "extract_claims") {
        return "提取关键声明";
    }
    if (name == "match_evidence") {
        return "匹配证据";
    }
    if (name == "check_consistency") {
        return "检查一致性";
    }
    if (name == "run_rules") {
        return "执行规则";
    }
    if (name == "calculate_trust_score") {
        return "计算评分";
    }
    if (name == "generate_fix_tasks") {
        return "生成补证任务";
    }
    if (name == "generate_repair_plan") {
        return "整理修复计划";
    }
    return stringText(name);
}

[[nodiscard]] bool containsAny(const QString& text, const QStringList& needles) {
    for (const auto& needle : needles) {
        if (text.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
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
    auditTimer_.setInterval(420);
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
    return workbench::permissionCards(llmApproved_);
}

QVariantList CompileController::artifacts() const {
    return workbench::artifacts(result_, auditDiff_, llmAdvice_);
}

QString CompileController::advisorSummary() const {
    return workbench::advisorSummary(result_);
}

bool CompileController::agentRunning() const {
    return agentRunning_;
}

int CompileController::agentProgress() const {
    const auto total = static_cast<int>(auditToolFlow().size()) + 1;
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
    return llmApiKey_;
}

void CompileController::setLlmApiKey(const QString& value) {
    if (llmApiKey_ == value) {
        return;
    }
    llmApiKey_ = value;
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

QString CompileController::llmAdvice() const {
    return llmAdvice_;
}

void CompileController::runAudit() {
    if (agentRunning_) {
        status_ = "审计正在进行";
        emit statusChanged();
        return;
    }
    if (normalizedInputPath(projectPath_).trimmed().isEmpty()) {
        status_ = "请先选择项目材料包";
        conversation_.push_back({"顾问", "先把项目目录或压缩包拖进来，我再开始审计。", "等待材料"});
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
         "我会按受控流程完成：建立安全工作副本、整理材料、读取文本、抽取声明、匹配证据、执行规则、计算评分，并生成补证计划。整个过程不覆盖原项目。",
         "审计计划"});
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
    auditTimer_.start();
}

void CompileController::advanceAuditRun() {
    const auto& flow = auditToolFlow();
    if (activeAuditStep_ + 1 < static_cast<int>(flow.size())) {
        if (activeAuditStep_ >= 0) {
            completedAuditSteps_ = activeAuditStep_ + 1;
        }
        ++activeAuditStep_;
        const auto label = toolLabel(flow[static_cast<std::size_t>(activeAuditStep_)]);
        currentAgentAction_ = label;
        status_ = QStringLiteral("正在%1").arg(label);
        conversation_.push_back({"工具", QStringLiteral("正在%1…").arg(label), label});
        emit statusChanged();
        emit agentStateChanged();
        emit workspaceChanged();
        emit sessionChanged();
        return;
    }

    auditTimer_.stop();
    completedAuditSteps_ = static_cast<int>(flow.size());
    activeAuditStep_ = -1;
    currentAgentAction_ = "汇总审计结果";
    status_ = "正在汇总审计结果";
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    finishAuditRun();
}

void CompileController::finishAuditRun() {
    cc::AuditOptions options;
    options.rulesDir = resolveRulesDir(projectPath_);
    const auto normalizedPath = normalizedInputPath(projectPath_);
    auto result = cc::AuditPipeline{}.run(normalizedPath.toStdString(), options);
    if (!result.ok()) {
        agentRunning_ = false;
        currentAgentAction_ = "审计失败";
        status_ = QString::fromStdString(result.error());
        conversation_.push_back({"系统", status_, "审计失败"});
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
    completedAuditSteps_ = static_cast<int>(auditToolFlow().size());
    currentAgentAction_ = "审计完成";
    status_ = "审计完成";
    conversation_.push_back({"工具", "已完成材料整理、证据匹配、规则检查和评分。",
                             QStringLiteral("会话 %1").arg(stringText(result_->context.sessionId))});
    conversation_.push_back({"顾问", advisorSummary(), "下一步建议"});
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
    conversation_.push_back(
        {"工具", "已基于两份审计数据包生成二次审计差分。", "差分完成"});
    emit statusChanged();
    emit auditDiffChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::runLlmAdvice() {
    if (!result_.has_value()) {
        status_ = "请先运行审计";
        emit statusChanged();
        return;
    }
    cc::LlmConfig config;
    config.apiKey = llmApiKey_.toStdString();
    config.endpoint = llmEndpoint_.toStdString();
    config.model = llmModel_.toStdString();
    config.allowNetwork = llmApproved_;
    config.allowLlm = llmApproved_;
    auto advice = cc::LlmBrain{}.advise(config, *result_);
    if (!advice.ok()) {
        status_ = QString::fromStdString(advice.error());
        emit statusChanged();
        return;
    }
    llmAdvice_ = QString::fromStdString(advice.value().content);
    status_ = "LLM Brain 建议已生成";
    conversation_.push_back(
        {"Brain", "已生成显式授权的大模型建议；该建议不参与最终评分。", "已授权联网"});
    emit statusChanged();
    emit llmAdviceChanged();
    emit workspaceChanged();
    emit sessionChanged();
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
        conversation_.push_back({"产物",
                                 QStringLiteral("Markdown 报告已导出到：%1").arg(outputPath),
                                 "报告已导出"});
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
        conversation_.push_back({"产物",
                                 QStringLiteral("JSON 审计包已导出到：%1").arg(outputPath),
                                 "数据包已导出"});
        emit workspaceChanged();
        emit sessionChanged();
    }
    emit statusChanged();
}

void CompileController::submitMessage(const QString& message) {
    const auto trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    conversation_.push_back({"用户", trimmed, "来自输入框"});
    const auto lower = trimmed.toLower();
    const bool wantsAudit = containsAny(lower, {"开始", "重新", "再跑", "运行", "审计"}) &&
                            containsAny(lower, {"审计", "检查", "跑", "运行"});
    if (wantsAudit) {
        emit sessionChanged();
        runAudit();
        return;
    }
    conversation_.push_back({"顾问", advisorReply(trimmed), "基于当前审计结果"});
    status_ = result_.has_value() ? "已生成顾问回复" : "请先运行审计";
    emit statusChanged();
    emit sessionChanged();
}

QString CompileController::advisorReply(const QString& message) const {
    if (agentRunning_) {
        return QStringLiteral("我正在%1。等这轮工具调用完成后，我会基于结果继续解释。")
            .arg(currentAgentAction_.isEmpty() ? "审计" : currentAgentAction_);
    }
    if (!result_.has_value()) {
        return "我还没有项目审计结果。先选择材料包并开始审计，我才能基于规则和证据回答。";
    }

    const auto lower = message.toLower();
    if (containsAny(lower, {"缺", "补证", "材料", "下一步"})) {
        if (result_->fixTasks.empty()) {
            return "当前没有生成补证任务。材料、证据和规则检查都没有发现必须补齐的项目。";
        }
        QStringList tasks;
        const auto count = std::min<std::size_t>(result_->fixTasks.size(), 3U);
        for (std::size_t index = 0; index < count; ++index) {
            tasks.push_back(stringText(result_->fixTasks[index].title));
        }
        return "优先补这些材料：" + tasks.join("；") + "。补完后再跑一次审计看差分。";
    }

    if (containsAny(lower, {"风险", "问题", "扣分", "不通过"})) {
        if (result_->findings.empty()) {
            return QStringLiteral("这轮没有命中规则风险，可信评分 %1/100。建议重点检查原始材料是否确实可公开、可提交。")
                .arg(result_->trustScore.totalScore);
        }
        const auto& finding = result_->findings.front();
        return QStringLiteral("优先看这个风险：%1。原因是：%2 建议：%3")
            .arg(stringText(finding.title), stringText(finding.reason),
                 stringText(finding.fixSuggestion));
    }

    if (containsAny(lower, {"答辩", "问题", "老师会问", "评委"})) {
        QStringList questions;
        questions << "这些用户、收入、合作或实验数据分别由哪些材料支撑？"
                  << "如果评委要求现场复核，你能否快速定位原始证据？"
                  << "项目商业模式、技术路线和当前成果之间是否口径一致？";
        if (!result_->fixTasks.empty()) {
            questions << QStringLiteral("补证任务“%1”为什么还没有闭环？")
                             .arg(stringText(result_->fixTasks.front().title));
        }
        return "可以准备这些答辩问题：" + questions.join("；");
    }

    if (containsAny(lower, {"导出", "报告", "json", "markdown"})) {
        return "可以到“报告导出”页生成可阅读报告和审计数据包；我不会自动覆盖你的原始项目。";
    }

    return advisorSummary();
}
