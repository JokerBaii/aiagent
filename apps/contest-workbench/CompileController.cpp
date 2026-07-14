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
#include "cc/llm/BrainAgentLoop.hpp"
#include "cc/llm/LlmBrain.hpp"
#include "cc/llm/LlmProviderProfile.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/report/MarkdownReporter.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/TimeUtil.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <set>
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

[[nodiscard]] QString naturalFindingTitle(const cc::AuditFinding& finding) {
    auto title = stringText(finding.title).trimmed();
    if (title == "材料文本无法可靠读取") {
        return "有文件没有读完整";
    }
    if (title.endsWith("失败")) {
        title.chop(2);
    }
    if (title.endsWith("检查")) {
        title.chop(2);
    }
    return title.isEmpty() ? QStringLiteral("这项材料需要补充") : title;
}

[[nodiscard]] QString naturalFindingReason(const std::string& rawReason) {
    auto reason = stringText(rawReason).trimmed();
    const auto internalDetails = reason.indexOf(" 缺失/风险项:");
    if (internalDetails >= 0) {
        reason = reason.left(internalDetails).trimmed();
    }
    reason.replace("NEED_REVIEW_TEXT_TRUNCATED", "文件只读取到一部分");
    reason.replace("NEED_REVIEW_STRUCTURED_TEXT_TRUNCATED", "文件只读取到一部分");
    reason.replace("NEED_REVIEW_PDF_TRUNCATED", "PDF 只读取到一部分");
    reason.replace("NEED_REVIEW_PDF_TEXT_EXTRACTION_LIMITED", "PDF 正文没有完整读出");
    reason.replace("NEED_REVIEW_OPENXML_TEXT_EXTRACTION_LIMITED", "文档正文没有完整读出");
    reason.replace("NEED_REVIEW_EXTRACTION_FAILED", "文件内容读取失败");
    reason.replace("EMPTY_OR_UNREADABLE", "没有读到可用文字");
    reason.replace("项目包中", "项目包里");
    return reason;
}

[[nodiscard]] QString readableExtractionStatus(const std::string& status) {
    if (status == "EXTRACTED_TEXT" || status == "EXTRACTED_STRUCTURED_JSON" ||
        status == "EXTRACTED_STRUCTURED_YAML") {
        return "已读取文本";
    }
    if (status == "EXTRACTED_PDF") {
        return "已读取 PDF 正文";
    }
    if (status == "EXTRACTED_OPENXML") {
        return "已读取文档正文";
    }
    if (status.find("TRUNCATED") != std::string::npos) {
        return "文件较大，只预览前一部分";
    }
    if (status.find("EXTRACTION_LIMITED") != std::string::npos) {
        return "正文没有完整读出";
    }
    if (status == "EMPTY_OR_UNREADABLE") {
        return "没有读到可用文字";
    }
    if (status.find("PARSE_FAILED") != std::string::npos) {
        return "文件格式需要检查";
    }
    if (status.find("NEED_REVIEW") != std::string::npos) {
        return "需要人工确认内容";
    }
    return status.empty() ? QStringLiteral("等待检查") : stringText(status);
}

[[nodiscard]] QString friendlyToolName(const std::string& name) {
    if (name == "run_project_audit") {
        return "运行项目规则审计";
    }
    if (name == "inventory_project") {
        return "整理项目文件";
    }
    if (name == "extract_text") {
        return "读取材料内容";
    }
    if (name == "detect_competition_type") {
        return "判断项目类型";
    }
    if (name == "build_cpir") {
        return "整理项目信息";
    }
    if (name == "extract_claims") {
        return "找出需要举证的成果";
    }
    if (name == "match_evidence") {
        return "核对证明材料";
    }
    if (name == "check_consistency") {
        return "核对材料表述";
    }
    if (name == "run_rules") {
        return "查找提交问题";
    }
    if (name == "calculate_trust_score") {
        return "计算分数";
    }
    if (name == "generate_fix_tasks") {
        return "整理修改清单";
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
    if (name == "read_external_text_file") {
        return "读取外部文件";
    }
    if (name == "write_project_file") {
        return "修改原项目文件";
    }
    if (name == "execute_shell_command") {
        return "执行 Shell 命令";
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
    const auto blockers = workbench::blockerCount(result);
    const auto warnings = workbench::warningCount(result);
    if (blockers > 0) {
        auto opening = QStringLiteral("我检查完了，目前是 %1 分。提交前有 %2 个问题要先处理")
                           .arg(result.trustScore.totalScore)
                           .arg(blockers);
        if (warnings > 0) {
            opening += QStringLiteral("，另外有 %1 个地方建议补齐").arg(warnings);
        }
        lines << opening + "。";
    } else if (warnings > 0) {
        lines << QStringLiteral("我检查完了，目前是 %1 分。没有发现会直接卡住提交的问题，"
                                "不过还有 %2 个地方值得补齐。")
                     .arg(result.trustScore.totalScore)
                     .arg(warnings);
    } else {
        lines << QStringLiteral("我检查完了，目前是 %1 分。从现有材料看，没有发现会直接影响"
                                "提交的问题。")
                     .arg(result.trustScore.totalScore);
    }

    if (result.findings.empty() && result.consistencyIssues.empty() && result.fixTasks.empty()) {
        lines << "提交前再人工看一遍关键数字、证明材料和文件版本，就可以了。";
        return lines.join("\n");
    }

    std::set<std::string> coveredRules;
    if (!result.findings.empty()) {
        lines << "\n先改这几处：";
        const auto limit = std::min<std::size_t>(result.findings.size(), 6U);
        for (std::size_t index = 0U; index < limit; ++index) {
            const auto& finding = result.findings.at(index);
            coveredRules.insert(finding.ruleId);
            const auto priority = finding.severity == cc::Severity::Blocker
                                      ? QStringLiteral("提交前处理")
                                      : QStringLiteral("建议补齐");
            lines << QStringLiteral("%1. %2（%3）")
                         .arg(static_cast<int>(index + 1U))
                         .arg(naturalFindingTitle(finding), priority);
            lines << QStringLiteral("   %1").arg(naturalFindingReason(finding.reason));
            if (!finding.fixSuggestion.empty()) {
                lines
                    << QStringLiteral("   可以这样处理：%1").arg(stringText(finding.fixSuggestion));
            }
        }
        if (result.findings.size() > limit) {
            lines << QStringLiteral("其余 %1 个问题可以在右侧“检查结果”里逐项查看。")
                         .arg(static_cast<int>(result.findings.size() - limit));
        }
    }

    if (!result.consistencyIssues.empty()) {
        lines << "\n还有几处材料表述需要对齐：";
        const auto limit = std::min<std::size_t>(result.consistencyIssues.size(), 3U);
        for (std::size_t index = 0U; index < limit; ++index) {
            const auto& issue = result.consistencyIssues.at(index);
            auto text = QStringLiteral("• %1").arg(stringText(issue.description));
            if (!issue.fixSuggestion.empty()) {
                text += QStringLiteral(" 可以这样处理：%1").arg(stringText(issue.fixSuggestion));
            }
            lines << text;
        }
    }

    QStringList extraTasks;
    for (const auto& task : result.fixTasks) {
        const auto alreadyCovered =
            std::any_of(task.affectedRules.begin(), task.affectedRules.end(),
                        [&](const std::string& rule) { return coveredRules.contains(rule); });
        if (alreadyCovered || extraTasks.size() >= 3) {
            continue;
        }
        extraTasks << QStringLiteral("• %1").arg(stringText(task.title));
    }
    if (!extraTasks.empty()) {
        lines << "\n另外可以补上：";
        lines.append(extraTasks);
    }

    lines << "\n如果你想让我直接整理或修改项目，直接说明目标即可；当前完全访问模式会在"
             "用户选择的原项目上工作。";
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
    constexpr const char* names[]{"LLM_PROVIDER", "DEEPSEEK_API_KEY", "DEEPSEEK_AUTH_TOKEN",
                                  "DEEPSEEK_BASE_URL", "DEEPSEEK_MODEL"};
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
    if (!profile.config.apiKey.empty() && !profile.config.endpoint.empty()) {
        llmCredentialConfig_ = profile.config;
    }
    if (profile.configured && cc::LlmProviderResolver{}.validateConfig(profile.config).ok()) {
        resolvedLlmConfig_ = profile.config;
        llmConfigured_ = true;
    } else if (!profile.error.empty()) {
        status_ =
            QStringLiteral("LLM 环境配置未启用：%1").arg(QString::fromStdString(profile.error));
    }
}

QString CompileController::projectPath() const {
    return projectPath_;
}

QString CompileController::projectDirectory() const {
    const auto normalized = normalizedInputPath(projectPath_).trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    const QFileInfo projectInfo(normalized);
    return QDir::cleanPath(projectInfo.isDir() ? projectInfo.absoluteFilePath()
                                               : projectInfo.absolutePath());
}

QUrl CompileController::projectDirectoryUrl() const {
    const auto directory = projectDirectory();
    return directory.isEmpty() ? QUrl{} : QUrl::fromLocalFile(directory);
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
    return workbench::permissionCards(llmConfigured_, accessMode_);
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
    if (agentRunning_) {
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
    llmCredentialConfig_.reset();
    refreshLlmConfig(true);
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
    refreshLlmConfig(true);
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
    refreshLlmConfig(false);
    emit llmConfigChanged();
    emit workspaceChanged();
}

bool CompileController::llmConfigured() const {
    return llmConfigured_;
}

QVariantList CompileController::llmAvailableModels() const {
    return llmAvailableModels_;
}

bool CompileController::llmModelsLoading() const {
    return llmModelsLoading_;
}

QString CompileController::llmModelsStatus() const {
    return llmModelsStatus_;
}

void CompileController::refreshLlmConfig(bool invalidateModels) {
    if (invalidateModels) {
        if (llmModelsCancellation_) {
            llmModelsCancellation_->store(true);
            llmModelsCancellation_.reset();
        }
        llmAvailableModels_.clear();
        llmModelsStatus_.clear();
        llmModelsLoading_ = false;
        emit llmModelsChanged();
    }
    resolvedLlmConfig_.reset();
    llmConfigured_ = false;
    if (llmApiKey_.isEmpty() || llmEndpoint_.trimmed().isEmpty() || llmModel_.trimmed().isEmpty()) {
        return;
    }
    auto resolved = cc::LlmProviderResolver{}.resolveUserProfile(
        llmEndpoint_.toStdString(), llmModel_.toStdString(), llmApiKey_.toStdString());
    if (!resolved.ok() || !resolved.value().configured) {
        return;
    }
    auto config = resolved.value().config;
    if (llmCredentialConfig_.has_value() && llmCredentialConfig_->apiKey == config.apiKey &&
        llmCredentialConfig_->provider == config.provider) {
        config.apiKeyHeader = llmCredentialConfig_->apiKeyHeader;
        config.apiKeyPrefix = llmCredentialConfig_->apiKeyPrefix;
    }
    llmCredentialConfig_ = config;
    resolvedLlmConfig_ = std::move(config);
    llmConfigured_ = true;
}

void CompileController::refreshLlmModels() {
    if (llmModelsLoading_) {
        return;
    }
    if (llmEndpoint_.trimmed().isEmpty() || llmApiKey_.isEmpty()) {
        llmModelsStatus_ = "请先填写 HTTPS 服务地址和访问密钥";
        emit llmModelsChanged();
        return;
    }
    auto profile = cc::LlmProviderResolver{}.resolveModelDiscoveryProfile(
        llmEndpoint_.toStdString(), llmApiKey_.toStdString());
    if (!profile.ok() || !profile.value().configured) {
        llmModelsStatus_ = profile.ok() ? QStringLiteral("访问密钥长度或格式无效")
                                        : QString::fromStdString(profile.error());
        emit llmModelsChanged();
        return;
    }

    auto config = profile.value().config;
    if (llmCredentialConfig_.has_value() && llmCredentialConfig_->apiKey == config.apiKey &&
        llmCredentialConfig_->provider == config.provider) {
        config.apiKeyHeader = llmCredentialConfig_->apiKeyHeader;
        config.apiKeyPrefix = llmCredentialConfig_->apiKeyPrefix;
    }
    llmCredentialConfig_ = config;
    config.allowNetwork = true;
    config.allowLlm = true;
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    llmModelsCancellation_ = cancellation;
    config.isCancelled = [cancellation]() { return cancellation->load(); };
    llmAvailableModels_.clear();
    llmModelsStatus_ = "正在读取该凭证可用的模型…";
    llmModelsLoading_ = true;
    emit llmModelsChanged();

    const QPointer<CompileController> guard{this};
    auto* worker = QThread::create([guard, config = std::move(config), cancellation]() mutable {
        auto modelResult =
            cc::Result<std::vector<std::string>>::failure("DeepSeek 模型目录读取尚未开始");
        try {
            modelResult = cc::LlmBrain{}.listModels(config);
        } catch (const std::exception&) {
            modelResult = cc::Result<std::vector<std::string>>::failure(
                "DeepSeek 模型目录读取发生未预期异常");
        } catch (...) {
            modelResult =
                cc::Result<std::vector<std::string>>::failure("DeepSeek 模型目录读取发生未知异常");
        }
        auto models =
            std::make_shared<cc::Result<std::vector<std::string>>>(std::move(modelResult));
        if (guard.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            guard,
            [guard, models, cancellation]() {
                if (guard.isNull() || cancellation->load() ||
                    guard->llmModelsCancellation_ != cancellation) {
                    return;
                }
                guard->llmModelsCancellation_.reset();
                guard->llmModelsLoading_ = false;
                guard->llmAvailableModels_.clear();
                if (!models->ok()) {
                    guard->llmModelsStatus_ = QStringLiteral("获取模型失败：%1")
                                                  .arg(QString::fromStdString(models->error()));
                } else {
                    for (const auto& model : models->value()) {
                        guard->llmAvailableModels_.push_back(QString::fromStdString(model));
                    }
                    guard->llmModelsStatus_ =
                        QStringLiteral("已读取 %1 个模型；请选择或继续手动输入")
                            .arg(guard->llmAvailableModels_.size());
                }
                emit guard->llmModelsChanged();
            },
            Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

cc::LlmConfig CompileController::llmConfig(bool allowNetwork, bool allowLlm) const {
    auto config = resolvedLlmConfig_.value_or(cc::LlmConfig{});
    config.allowNetwork = allowNetwork;
    config.allowLlm = allowLlm;
    return config;
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
    const auto normalized =
        key == "plan" || key == "manual" || key == "readonly" || key == "read-only"
            ? QStringLiteral("plan")
            : QStringLiteral("full");
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
    agentResult_.clear();
    agentTrace_.clear();
    accessMode_ = "full";
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
        conversation_.push_back({"系统",
                                 "已开始新任务。请添加完整项目文件夹、项目压缩包或单个项目文件。",
                                 "会话", "system", QString{}, true});
    }
}

void CompileController::emitFullSessionState() {
    emit projectPathChanged();
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
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
    if (agentRunning_) {
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
    const bool brainReady = llmConfigured_ && !llmApiKey_.isEmpty();
    status_ = brainReady ? "大模型审计助手正在分析项目" : "正在启动本地规则审计";
    emit statusChanged();
    emit resultChanged();
    emit auditDiffChanged();
    emit agentResultChanged();
    emit agentTraceChanged();
    emit selectedFilePreviewChanged();
    emit workspaceChanged();
    emit sessionChanged();
    if (brainReady) {
        startDeferredAgentConversation(
            "请审查当前项目。可以先直接读取和搜索用户选择的原项目；给出最终评分前调用 "
            "run_project_audit 获取确定性规则、证据匹配和评分结果，最后回答主要缺点和下一步。",
            "项目导入", false);
        return;
    }
    runAudit();
}

void CompileController::newSession() {
    if (agentRunning_) {
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
            item["subtitle"] = QStringLiteral("%1 分，%2 个问题要处理")
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
    if (agentRunning_ || sessionId.isEmpty() || sessionId == activeSessionId_) {
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
    accessMode_ = "full";
    agentResult_ = std::move(restored.agentResult);
    agentTrace_ = std::move(restored.agentTrace);
    pendingPlanGoal_ = std::move(restored.pendingPlanGoal);
    compactedContext_ = std::move(restored.compactedContext);
    selectedFilePreview_ = std::move(restored.selectedFilePreview);
    result_ = std::move(restored.result);
    baselineResult_ = std::move(restored.baselineResult);
    auditDiff_ = std::move(restored.auditDiff);
    conversation_ = std::move(restored.conversation);
    activeAuditStep_ = -1;
    completedAuditSteps_ = result_ != nullptr ? static_cast<int>(auditStageCount()) : 0;
    currentAgentAction_.clear();
    emitFullSessionState();
}

QVariantMap CompileController::selectedFilePreview() const {
    return selectedFilePreview_;
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

    if (!llmConfigured_ || llmApiKey_.isEmpty()) {
        status_ = "常规问答需要先在设置中配置 DeepSeek";
        conversation_.push_back(
            {"智能体",
             "我可以作为常规问答助手，也可以专门评审竞赛项目。当前没有配置 DeepSeek，所以"
             "不能生成开放式回答；你可以先拖入项目让我做规则评审，或在设置里填入 API key 后再问。",
             "常规问答", "assistant", QString{}, true});
        emit statusChanged();
        emit sessionChanged();
        return;
    }

    auto config = llmConfig(true, true);
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
            auto responseValue = cc::Result<cc::LlmResponse>::failure("DeepSeek 问答尚未开始");
            try {
                responseValue = cc::LlmBrain{}.complete(config, messages);
            } catch (const std::exception&) {
                responseValue = cc::Result<cc::LlmResponse>::failure("DeepSeek 问答发生未预期异常");
            } catch (...) {
                responseValue = cc::Result<cc::LlmResponse>::failure("DeepSeek 问答发生未知异常");
            }
            auto response = std::make_shared<cc::Result<cc::LlmResponse>>(std::move(responseValue));
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
                        guard->status_ = friendlyFailure(response->error(), "常规问答");
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
    if (agentRunning_) {
        status_ = "审计正在进行";
        emit statusChanged();
        return;
    }
    if (normalizedInputPath(projectPath_).trimmed().isEmpty()) {
        status_ = "请先添加项目文件";
        conversation_.push_back({"智能体",
                                 "先添加完整项目文件夹、项目压缩包或单个项目文件，我再开始检查。",
                                 "等待项目"});
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
    conversation_.push_back({"工具", "材料已经整理完，证据、规则和评分也都核对过了。", "检查记录"});
    conversation_.push_back(
        {"智能体", defectReportText(*result_), "检查结论", "assistant", QString{}, true});
    conversation_.push_back({"产物", "打开检查总览、材料、证明、问题和修改清单。", "完整检查结果",
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
    if (agentRunning_) {
        status_ = "请等当前任务结束后再比较";
        emit statusChanged();
        return;
    }
    if (oldAuditPath_.isEmpty() || newAuditPath_.isEmpty()) {
        status_ = "请先选择修改前、修改后的两份检查结果";
        emit statusChanged();
        return;
    }
    const auto oldNormalized = normalizedInputPath(oldAuditPath_).trimmed();
    const auto newNormalized = normalizedInputPath(newAuditPath_).trimmed();
    const QFileInfo oldInfo{oldNormalized};
    const QFileInfo newInfo{newNormalized};
    if (!oldInfo.isFile() || !newInfo.isFile()) {
        status_ = !oldInfo.isFile() ? "找不到修改前的检查结果，请重新选择"
                                    : "找不到修改后的检查结果，请重新选择";
        emit statusChanged();
        return;
    }
    if (oldInfo.absoluteFilePath() == newInfo.absoluteFilePath()) {
        status_ = "修改前和修改后不能选择同一个文件";
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

    const auto oldPath = oldNormalized.toStdString();
    const auto newPath = newNormalized.toStdString();
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
    startDeferredAgentConversation(trimmed, "DeepSeek 任务", true);
}

QString CompileController::accessModeLabel() const {
    if (accessMode_ == "full") {
        return "完全访问模式";
    }
    if (accessMode_ == "plan") {
        return "Plan 模式";
    }
    return "完全访问模式";
}

QString CompileController::sessionStatusText() const {
    QStringList lines;
    const auto normalized = normalizedInputPath(projectPath_).trimmed();
    lines << QStringLiteral("权限模式：%1").arg(accessModeLabel());
    lines << QStringLiteral("项目文件：%1").arg(normalized.isEmpty() ? "未选择" : normalized);
    lines << QStringLiteral("DeepSeek：%1")
                 .arg(llmConfigured_ && !llmApiKey_.isEmpty() ? "配置有效" : "本地受控");
    lines << QStringLiteral("当前状态：%1").arg(status_);
    if (result_ != nullptr) {
        lines << QStringLiteral("检查结果：%1 分，%2 个问题要处理，%3 个地方建议补齐，"
                                "%4 项修改任务")
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
    if (agentRunning_) {
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
    if (agentRunning_) {
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
        extractionStatus = readableExtractionStatus(document->status);
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

    const bool brainReady = llmConfigured_ && !llmApiKey_.isEmpty();
    status_ = brainReady ? "大模型审计助手正在分析" : "本地诊断：未启用大模型";
    emit statusChanged();

    if (brainReady) {
        auto brainConfig = llmConfig(request.allowNetwork, request.allowLlm);
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
            auto run = cc::Result<cc::AgentRunResult>::failure("DeepSeek 运行尚未开始");
            try {
                run = cc::BrainAgentLoop{}.run(brainConfig, requestCopy, streamEvent);
            } catch (const std::exception&) {
                run = cc::Result<cc::AgentRunResult>::failure(
                    "DeepSeek 运行时发生未预期异常，已进入安全恢复");
            } catch (...) {
                run = cc::Result<cc::AgentRunResult>::failure(
                    "DeepSeek 运行时发生未知异常，已进入安全恢复");
            }
            bool usedLocalFallback = false;
            if (!run.ok() && !cancellation->load()) {
                usedLocalFallback = true;
                auto fallbackRequest = requestCopy;
                const bool requestedChanges =
                    fallbackRequest.requireWorkspaceChanges || fallbackRequest.requireReaudit;
                // A provider/format failure must not take down the agent runtime. Preserve the
                // strict "do not pretend edits succeeded" invariant, but still run deterministic
                // local inspection and the mandatory audit when possible.
                fallbackRequest.requireWorkspaceChanges = false;
                fallbackRequest.requireReaudit = false;
                auto fallback = cc::Result<cc::AgentRunResult>::failure("本地降级尚未开始");
                try {
                    fallback = cc::AgentRuntime{}.runLocal(fallbackRequest);
                } catch (const std::exception&) {
                    fallback =
                        cc::Result<cc::AgentRunResult>::failure("本地降级运行时发生未预期异常");
                } catch (...) {
                    fallback = cc::Result<cc::AgentRunResult>::failure("本地降级发生未知异常");
                }
                if (fallback.ok()) {
                    fallback.value().plan.summary =
                        "模型决策暂时不可用，已自动切换到本地确定性工具。";
                    fallback.value().events.insert(
                        fallback.value().events.begin(),
                        cc::AgentEvent{.kind = cc::AgentEventKind::System,
                                       .role = "系统",
                                       .text = "模型响应未通过校验，已自动完成本地降级处理。",
                                       .context = "自动恢复",
                                       .payload = cc::JsonValue::Object{{"fallback", true}}});
                    const auto localDetail = fallback.value().finalAnswer;
                    const auto recoveryAnswer =
                        requestedChanges
                            ? std::string{"模型服务本轮未能稳定返回决策，已自动完成本地规则检查。"
                                          "为避免错误改写，未把本轮任务标记为修改完成；请稍后重试，"
                                          "现有文件和已生成的检查结果均已保留。"}
                            : std::string{"模型服务本轮未能稳定返回决策，已自动切换到本地工具完成"
                                          "可复核检查。\n"} +
                                  localDetail;
                    cc::setAgentFinalAnswer(fallback.value(), recoveryAnswer, "本地自动降级");
                    run = std::move(fallback);
                } else if (fallback.error().find("取消") == std::string::npos) {
                    cc::AgentRunResult recovered;
                    recovered.plan.summary = "模型与本地工具均暂时不可用，运行时已安全收束。";
                    recovered.events.push_back(
                        cc::AgentEvent{.kind = cc::AgentEventKind::System,
                                       .role = "系统",
                                       .text = "本轮已安全停止；没有把未完成操作标记为成功。",
                                       .context = "自动恢复",
                                       .payload = cc::JsonValue::Object{{"fallback", true}}});
                    cc::setAgentFinalAnswer(
                        recovered,
                        "智能体已从异常中安全恢复，但本轮没有得到足够结果。项目文件保持原状，"
                        "请检查模型配置或稍后重试。",
                        "安全收束");
                    run = cc::Result<cc::AgentRunResult>::success(std::move(recovered));
                } else {
                    run = std::move(fallback);
                }
            }
            if (run.ok() && run.value().auditResult.has_value()) {
                persistAuditPackage(run.value().auditResult.value());
            }
            auto outcome = std::make_shared<cc::Result<cc::AgentRunResult>>(std::move(run));
            if (guard.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                guard,
                [guard, outcome, sessionId, cancellation, usedLocalFallback]() mutable {
                    if (guard.isNull() || cancellation->load() ||
                        guard->activeSessionId_ != sessionId ||
                        guard->activeCancellation_ != cancellation) {
                        return;
                    }
                    guard->brainWorkerRunning_ = false;
                    guard->applyAgentRunResult(std::move(*outcome),
                                               usedLocalFallback ? "本地降级" : "DeepSeek",
                                               usedLocalFallback);
                    guard->finishDeferredAgentConversation();
                },
                Qt::QueuedConnection);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
        return;
    }

    conversation_.push_back({"系统", "未检测到有效的 DeepSeek 配置，本轮只执行本地上下文诊断。",
                             "DeepSeek 未启用", "system", QString{}, true});
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
        auto run = cc::Result<cc::AgentRunResult>::failure("本地运行尚未开始");
        try {
            run = cc::AgentRuntime{}.runLocal(requestCopy);
        } catch (const std::exception&) {
            run = cc::Result<cc::AgentRunResult>::failure("本地运行时发生未预期异常");
        } catch (...) {
            run = cc::Result<cc::AgentRunResult>::failure("本地运行时发生未知异常");
        }
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
        status_ = friendlyFailure(run.error(), planner == "DeepSeek" ? "智能分析" : "当前操作");
        conversation_.push_back(
            {"系统",
             planner == "DeepSeek"
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
        selectedFilePreview_.clear();
        compactedContext_.clear();
        if (!producedDiff) {
            auditDiff_.reset();
        }
        completedAuditSteps_ = static_cast<int>(auditStageCount());
        activeAuditStep_ = -1;
        currentAgentAction_ = producedDiff ? "修改后复查完成" : "项目文件检查完成";
    }
    status_ = producedAudit           ? (producedDiff ? "修改后复查完成" : "项目文件检查完成")
              : planner == "DeepSeek" ? "DeepSeek 智能体已完成分析"
              : planner == "本地降级" ? "模型响应异常，已完成本地安全降级"
                                      : "仅完成本地上下文诊断";
    emit statusChanged();
    if (producedAudit) {
        emit resultChanged();
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

    if (agentRunning_) {
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
    if (!agentRunning_) {
        return;
    }
    if (activeCancellation_) {
        activeCancellation_->store(true);
        activeCancellation_.reset();
    }
    agentRunning_ = false;
    brainWorkerRunning_ = false;
    activeAuditStep_ = -1;
    completedAuditSteps_ = 0;
    currentAgentAction_.clear();
    pendingComposerMessages_.clear();
    status_ = "已停止当前任务";
    conversation_.push_back({"系统", "已停止当前任务，后台返回的过期结果不会写入会话。", "任务停止",
                             "system", QString{}, true});
    emit statusChanged();
    emit agentStateChanged();
    emit workspaceChanged();
    emit sessionChanged();
}

void CompileController::flushQueuedComposerMessage() {
    if (agentRunning_ || pendingComposerMessages_.empty()) {
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
    const bool fullAccess = accessMode_ != "plan";
    const bool brainReady = llmConfigured_ && !llmApiKey_.isEmpty();
    request.allowNetwork = brainReady || fullAccess;
    request.allowLlm = brainReady;
    request.allowWriteWorkspace = fullAccess;
    request.allowReadExternal = fullAccess;
    request.allowModifyOriginal = fullAccess;
    request.allowExecuteCommand = fullAccess;
    if (result_ != nullptr) {
        const auto instructions =
            cc::ProjectMemory{}.loadInstructions(result_->context.workspaceRoot);
        if (instructions.ok()) {
            request.projectInstructions = instructions.value();
        }
    }
    if (fullAccess) {
        request.projectInstructions +=
            "\n- 用户已显式选择完全访问模式：文件读取、文件写入、Shell/Bash、外部命令和"
            "网络权限全部开放，不设置固定命令超时或输出上限。\n";
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
        request.projectRoot = result_->context.originalRoot;
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
        request.requireAudit = context == "项目导入" || context == "/audit";
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
    if (agentRunning_) {
        status_ = "请等待当前任务结束后再导出报告";
        emit statusChanged();
        return;
    }
    auto normalized = normalizedInputPath(outputPath).trimmed();
    if (normalized.isEmpty()) {
        status_ = "请选择报告导出路径";
        emit statusChanged();
        return;
    }
    if (QDir::isRelativePath(normalized) && !projectDirectory().isEmpty()) {
        normalized = QDir(projectDirectory()).filePath(normalized);
    }
    normalized = QDir::cleanPath(normalized);

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
        if (llmConfigured_ && !llmApiKey_.isEmpty() &&
            !normalizedInputPath(projectPath_).trimmed().isEmpty()) {
            result_.reset();
            baselineResult_.reset();
            auditDiff_.reset();
            agentResult_.clear();
            agentTrace_.clear();
            emit resultChanged();
            emit auditDiffChanged();
            emit agentResultChanged();
            emit agentTraceChanged();
            startDeferredAgentConversation(
                "请重新审查当前项目。可以先直接读取原项目；最终回答前调用 "
                "run_project_audit，并依据规则和证据结果回答。",
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
