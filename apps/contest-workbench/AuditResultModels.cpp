/**
 * @file AuditResultModels.cpp
 * @brief 将 C++ Core 审计结果转换为 QML 可消费的 QVariant 模型。
 */

#include "AuditResultModels.hpp"

#include "cc/evidence/EvidenceMatcher.hpp"

#include <QStringList>

#include <algorithm>

namespace workbench {
namespace {

[[nodiscard]] QString pathText(const std::filesystem::path& path) {
    return QString::fromStdString(path.generic_string());
}

[[nodiscard]] QString stringText(const std::string& value) {
    return QString::fromStdString(value);
}

[[nodiscard]] QString joinedStrings(const std::vector<std::string>& values) {
    QStringList items;
    for (const auto& value : values) {
        items.push_back(stringText(value));
    }
    return items.join("、");
}

[[nodiscard]] QString joinedPaths(const std::vector<std::filesystem::path>& values) {
    QStringList items;
    for (const auto& value : values) {
        items.push_back(pathText(value));
    }
    return items.join("、");
}

[[nodiscard]] QString assetRoleText(cc::AssetRole role) {
    switch (role) {
    case cc::AssetRole::ProjectDeclaration:
        return "项目申报材料";
    case cc::AssetRole::BusinessPlan:
        return "商业计划书";
    case cc::AssetRole::PitchDeck:
        return "路演材料";
    case cc::AssetRole::MarketResearch:
        return "市场调研";
    case cc::AssetRole::CompetitorAnalysis:
        return "竞品分析";
    case cc::AssetRole::FinancialPlan:
        return "财务预测";
    case cc::AssetRole::UserResearch:
        return "用户调研";
    case cc::AssetRole::SourceCode:
        return "源码";
    case cc::AssetRole::BuildSystem:
        return "构建入口";
    case cc::AssetRole::DependencyManifest:
        return "依赖清单";
    case cc::AssetRole::DeploymentDoc:
        return "部署说明";
    case cc::AssetRole::ExperimentData:
        return "实验数据";
    case cc::AssetRole::ResearchPaper:
        return "论文或研究说明";
    case cc::AssetRole::PatentCopyright:
        return "知识产权证明";
    case cc::AssetRole::ProofMaterial:
        return "成果证明";
    case cc::AssetRole::SocialPracticeProof:
        return "实践证明";
    case cc::AssetRole::ResourceAsset:
        return "资源素材";
    case cc::AssetRole::ModelArtifact:
        return "模型产物";
    case cc::AssetRole::BinaryArtifact:
        return "二进制产物";
    case cc::AssetRole::Archive:
        return "压缩包";
    case cc::AssetRole::Generated:
        return "生成物";
    case cc::AssetRole::Vendored:
        return "第三方依赖";
    case cc::AssetRole::SecretRisk:
        return "敏感文件";
    case cc::AssetRole::Unknown:
        return "未归类";
    }
    return "未归类";
}

[[nodiscard]] QString severityText(cc::Severity severity) {
    switch (severity) {
    case cc::Severity::Info:
        return "提示";
    case cc::Severity::Warning:
        return "需要关注";
    case cc::Severity::Blocker:
        return "必须处理";
    }
    return "需要关注";
}

[[nodiscard]] QString claimTypeText(cc::ClaimType type) {
    switch (type) {
    case cc::ClaimType::UserTraction:
        return "用户与使用数据";
    case cc::ClaimType::MarketScale:
        return "市场规模";
    case cc::ClaimType::TechnicalCapability:
        return "技术能力";
    case cc::ClaimType::BusinessModel:
        return "商业模式";
    case cc::ClaimType::Revenue:
        return "收入";
    case cc::ClaimType::CostReduction:
        return "降本增效";
    case cc::ClaimType::Patent:
        return "专利";
    case cc::ClaimType::Copyright:
        return "软著";
    case cc::ClaimType::Partnership:
        return "合作";
    case cc::ClaimType::Prototype:
        return "原型";
    case cc::ClaimType::ResearchResult:
        return "研究结果";
    case cc::ClaimType::SocialImpact:
        return "社会影响";
    case cc::ClaimType::Deployment:
        return "部署上线";
    case cc::ClaimType::Unknown:
        return "其他声明";
    }
    return "其他声明";
}

[[nodiscard]] QString evidenceStatusText(cc::EvidenceStatus status) {
    switch (status) {
    case cc::EvidenceStatus::Supported:
        return "证据充分";
    case cc::EvidenceStatus::Partial:
        return "证据不足";
    case cc::EvidenceStatus::Unsupported:
        return "缺少证据";
    case cc::EvidenceStatus::Conflicted:
        return "存在冲突";
    case cc::EvidenceStatus::NeedReview:
        return "需要复核";
    }
    return "需要复核";
}

[[nodiscard]] QString riskFlagText(const std::string& flag) {
    if (flag == "SECRET_RISK") {
        return "可能包含敏感信息";
    }
    if (flag == "GENERATED") {
        return "生成物";
    }
    if (flag == "VENDORED") {
        return "第三方依赖";
    }
    if (flag == "NESTED_ARCHIVE_NEEDS_REVIEW") {
        return "嵌套压缩包待复核";
    }
    if (flag == "CONTENT_DEFERRED") {
        return "内容未自动载入";
    }
    if (flag == "LARGE_BINARY_DEFERRED") {
        return "文件超过自动读取预算";
    }
    if (flag == "COPY_BUDGET_DEFERRED") {
        return "工作副本容量受限，暂保留元数据";
    }
    if (flag == "PATH_DEPTH_LIMIT") {
        return "路径层级过深，暂保留元数据";
    }
    if (flag == "SYMLINK_DEFERRED") {
        return "符号链接未跟随，等待确认";
    }
    if (flag == "FIFO_DEFERRED" || flag == "SOCKET_DEFERRED" ||
        flag == "BLOCK_DEVICE_DEFERRED" || flag == "CHARACTER_DEVICE_DEFERRED" ||
        flag == "NON_REGULAR_FILE_DEFERRED" || flag == "NON_REGULAR_ENTRY_DEFERRED") {
        return "非普通文件已识别，未打开内容";
    }
    if (flag == "NESTED_ARCHIVE_DEFERRED") {
        return "嵌套归档已识别，暂未继续展开";
    }
    if (flag == "ENCRYPTED_ENTRY_DEFERRED") {
        return "加密条目已识别，需要解密后读取";
    }
    if (flag == "ENCRYPTION_STATUS_UNKNOWN_DEFERRED") {
        return "无法确认条目加密状态，未读取内容";
    }
    if (flag == "UNSUPPORTED_COMPRESSION_DEFERRED" ||
        flag == "UNSUPPORTED_ARCHIVE_FORMAT") {
        return "格式已识别，当前解析器暂不支持内容读取";
    }
    if (flag == "ARCHIVE_TOO_LARGE_FOR_INDEXING") {
        return "归档超过自动展开预算，暂保留元数据";
    }
    if (flag == "RUNTIME_SINGLE_FILE_LIMIT_DEFERRED" ||
        flag == "RUNTIME_SIZE_LIMIT_DEFERRED") {
        return "条目实际大小超限，暂保留元数据";
    }
    if (flag == "RUNTIME_COPY_BUDGET_DEFERRED") {
        return "展开时达到工作副本预算，暂保留元数据";
    }
    if (flag == "FILE_METADATA_UNREADABLE" || flag == "COPY_FAILED") {
        return "文件暂不可读取，元数据已保留";
    }
    if (flag == "CONTENT_UNREADABLE") {
        return "文件内容暂不可读取";
    }
    if (flag == "CONTENT_FORMAT_DETECTED") {
        return "已根据文件内容识别格式";
    }
    if (flag == "WORKSPACE_DRAFT_UNVERIFIED") {
        return "修复草稿，等待人工确认";
    }
    return stringText(flag);
}

[[nodiscard]] QString joinedRiskFlags(const std::vector<std::string>& values) {
    QStringList items;
    for (const auto& value : values) {
        items.push_back(riskFlagText(value));
    }
    return items.join("、");
}

} // namespace

QString riskFlags(const std::vector<std::string>& values) {
    return joinedRiskFlags(values);
}

int blockerCount(const cc::AuditResult& result) {
    return static_cast<int>(std::count_if(
        result.findings.begin(), result.findings.end(),
        [](const cc::AuditFinding& finding) { return finding.severity == cc::Severity::Blocker; }));
}

int warningCount(const cc::AuditResult& result) {
    return static_cast<int>(std::count_if(
        result.findings.begin(), result.findings.end(),
        [](const cc::AuditFinding& finding) { return finding.severity == cc::Severity::Warning; }));
}

QString summary(const cc::AuditResult& result) {
    return QStringLiteral("资产 %1 个，声明 %2 条，补证任务 %3 个")
        .arg(result.inventory.assets.size())
        .arg(result.claims.size())
        .arg(result.fixTasks.size());
}

QVariantList assets(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& asset : result.inventory.assets) {
        QVariantMap item;
        item["path"] = pathText(asset.relativePath);
        item["role"] = assetRoleText(asset.role);
        item["roleCode"] = stringText(cc::toString(asset.role));
        item["format"] = stringText(asset.format);
        item["size"] = QVariant::fromValue<qulonglong>(asset.sizeBytes);
        item["auditable"] = asset.auditable;
        item["risk"] = riskFlags(asset.riskFlags);
        items.push_back(item);
    }
    return items;
}

QVariantList roleDistribution(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& [role, count] : result.inventory.roleCounts) {
        QVariantMap item;
        item["role"] = assetRoleText(role);
        item["roleCode"] = stringText(cc::toString(role));
        item["count"] = static_cast<int>(count);
        items.push_back(item);
    }
    return items;
}

QVariantMap cpir(const cc::AuditResult& result) {
    QVariantMap item;
    item["projectName"] = stringText(result.cpir.projectName);
    item["competitionType"] = stringText(cc::toString(result.cpir.competitionType));
    item["competitionConfidence"] = result.cpir.competitionConfidence;
    item["competitionReason"] = stringText(result.cpir.competitionReason);
    item["track"] = stringText(result.cpir.track);
    item["targetUser"] = stringText(result.cpir.targetUser);
    item["painPoint"] = stringText(result.cpir.painPoint);
    item["solution"] = stringText(result.cpir.solution);
    item["productOrService"] = stringText(result.cpir.productOrService);
    item["technicalRoute"] = stringText(result.cpir.technicalRoute);
    item["businessModel"] = stringText(result.cpir.businessModel);
    item["marketAnalysis"] = stringText(result.cpir.marketAnalysis);
    item["competitorAnalysis"] = stringText(result.cpir.competitorAnalysis);
    item["financialProjection"] = stringText(result.cpir.financialProjection);
    item["teamStructure"] = stringText(result.cpir.teamStructure);
    item["currentResults"] = stringText(result.cpir.currentResults);
    item["socialValue"] = stringText(result.cpir.socialValue);
    item["missingFields"] = joinedStrings(result.cpir.missingFields);
    item["riskItems"] = joinedStrings(result.cpir.riskItems);
    return item;
}

QVariantList claimEvidence(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& claim : result.claims) {
        const auto match = std::find_if(
            result.evidenceMatches.begin(), result.evidenceMatches.end(),
            [&](const cc::EvidenceMatch& item) { return item.claimId == claim.claimId; });
        QVariantMap item;
        item["claimId"] = stringText(claim.claimId);
        item["type"] = claimTypeText(claim.claimType);
        item["typeCode"] = stringText(cc::toString(claim.claimType));
        item["text"] = stringText(claim.claimText);
        item["source"] = pathText(claim.sourceFile);
        item["status"] = evidenceStatusText(cc::EvidenceStatus::NeedReview);
        item["reason"] = QStringLiteral("该声明尚未生成证据匹配结果。");
        item["evidence"] = QString{};
        item["missing"] = joinedStrings(cc::missingEvidenceForClaim(claim.claimType));
        if (match != result.evidenceMatches.end()) {
            item["status"] = evidenceStatusText(match->status);
            item["reason"] = stringText(match->reason);
            item["evidence"] = joinedPaths(match->evidenceFiles);
            item["missing"] = joinedStrings(match->missingEvidence);
        }
        items.push_back(item);
    }
    return items;
}

QVariantList consistencyIssues(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& issue : result.consistencyIssues) {
        QVariantMap item;
        item["issueId"] = stringText(issue.issueId);
        item["severity"] = severityText(issue.severity);
        item["severityCode"] = stringText(cc::toString(issue.severity));
        item["description"] = stringText(issue.description);
        item["files"] = joinedPaths(issue.affectedFiles);
        item["fix"] = stringText(issue.fixSuggestion);
        items.push_back(item);
    }
    return items;
}

QVariantList findings(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& finding : result.findings) {
        QVariantMap item;
        item["ruleId"] = stringText(finding.ruleId);
        item["severity"] = severityText(finding.severity);
        item["severityCode"] = stringText(cc::toString(finding.severity));
        item["title"] = stringText(finding.title);
        item["reason"] = stringText(finding.reason);
        item["evidence"] = joinedPaths(finding.evidence);
        item["missing"] = joinedStrings(finding.missingEvidence);
        item["fix"] = stringText(finding.fixSuggestion);
        items.push_back(item);
    }
    return items;
}

QVariantList fixTasks(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& task : result.fixTasks) {
        QVariantMap item;
        item["taskId"] = stringText(task.taskId);
        item["priority"] = stringText(task.priority);
        item["title"] = stringText(task.title);
        item["reason"] = stringText(task.reason);
        item["required"] = joinedStrings(task.requiredMaterial);
        item["rules"] = joinedStrings(task.affectedRules);
        item["files"] = joinedPaths(task.relatedFiles);
        items.push_back(item);
    }
    return items;
}

QVariantList scorePenalties(const cc::AuditResult& result) {
    QVariantList items;
    for (const auto& penalty : result.trustScore.penalties) {
        QVariantMap item;
        item["ruleId"] = stringText(penalty.ruleId);
        item["points"] = penalty.points;
        item["dimension"] = stringText(penalty.dimension);
        item["reason"] = stringText(penalty.reason);
        items.push_back(item);
    }
    return items;
}

QVariantMap auditDiff(const cc::AuditDiff& diff) {
    QVariantMap item;
    item["summary"] = stringText(diff.summary);
    item["oldScore"] = diff.oldScore;
    item["newScore"] = diff.newScore;
    item["oldTrustDebt"] = diff.oldTrustDebt;
    item["newTrustDebt"] = diff.newTrustDebt;
    item["oldBlockers"] = diff.oldBlockers;
    item["newBlockers"] = diff.newBlockers;
    item["oldWarnings"] = diff.oldWarnings;
    item["newWarnings"] = diff.newWarnings;
    item["oldEvidenceCoverage"] = diff.oldEvidenceCoverage;
    item["newEvidenceCoverage"] = diff.newEvidenceCoverage;
    item["oldMaterialCompleteness"] = diff.oldMaterialCompleteness;
    item["newMaterialCompleteness"] = diff.newMaterialCompleteness;
    item["oldConsistencyScore"] = diff.oldConsistencyScore;
    item["newConsistencyScore"] = diff.newConsistencyScore;
    item["oldFixTaskCount"] = diff.oldFixTaskCount;
    item["newFixTaskCount"] = diff.newFixTaskCount;
    return item;
}

} // namespace workbench
