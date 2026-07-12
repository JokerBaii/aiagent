/**
 * @file MarkdownReporter.cpp
 * @brief Markdown 可信审计报告导出实现。
 */

#include "cc/report/MarkdownReporter.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <sstream>

namespace cc {
namespace {

[[nodiscard]] std::string joinStrings(const std::vector<std::string>& values) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << (index == 0U ? "" : "、") << values[index];
    }
    return output.str();
}

[[nodiscard]] std::string joinPaths(const std::vector<std::filesystem::path>& values) {
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << (index == 0U ? "" : "、") << util::pathString(values[index]);
    }
    return output.str();
}

} // namespace

std::string MarkdownReporter::render(const AuditResult& result, const AuditDiff* diff) const {
    std::ostringstream output;
    output << "# 竞赛项目可信审计报告\n\n";
    output << "## 项目概况\n\n";
    output << "- 项目名称：" << result.cpir.projectName << "\n";
    output << "- 竞赛类型：" << toString(result.cpir.competitionType) << "\n";
    output << "- 类型置信度：" << result.cpir.competitionConfidence << "\n";
    output << "- 判断理由：" << result.cpir.competitionReason << "\n";
    output << "- 原始路径：" << util::pathString(result.context.originalRoot) << "\n";
    output << "- 工作区输入：" << util::pathString(result.context.inputRoot) << "\n";
    output << "- 导入状态：" << result.context.unpackStatus << "\n";
    output << "- 资产数量：" << result.inventory.assets.size() << "\n";
    output << "- 可信评分：" << result.trustScore.totalScore << "/100\n";
    output << "- 可信债务：" << result.trustScore.trustDebt << "\n\n";

    const auto ruleBlockers = std::count_if(
        result.findings.begin(), result.findings.end(),
        [](const AuditFinding& finding) { return finding.severity == Severity::Blocker; });
    const auto p0Tasks = std::count_if(result.fixTasks.begin(), result.fixTasks.end(),
                                       [](const FixTask& task) { return task.priority == "P0"; });
    output << "- 规则阻断项：" << ruleBlockers << "\n";
    output << "- 必须处理任务：" << p0Tasks << "\n\n";

    output << "## 资产清单\n\n";
    for (const auto& asset : result.inventory.assets) {
        output << "- `" << util::pathString(asset.relativePath) << "`：" << toString(asset.role)
               << "，格式 " << asset.format << "，MIME " << asset.mime;
        if (!asset.riskFlags.empty()) {
            output << "，风险 " << joinStrings(asset.riskFlags);
        }
        output << "\n";
    }
    if (result.inventory.assets.empty()) {
        output << "未发现可列出的项目资产。\n";
    }
    output << "\n## CPIR 项目中间表示\n\n";
    output << "- 目标用户：" << result.cpir.targetUser << "\n";
    output << "- 痛点：" << result.cpir.painPoint << "\n";
    output << "- 解决方案：" << result.cpir.solution << "\n";
    output << "- 产品或服务：" << result.cpir.productOrService << "\n";
    output << "- 技术路线：" << result.cpir.technicalRoute << "\n";
    output << "- 商业模式：" << result.cpir.businessModel << "\n";
    output << "- 市场分析：" << result.cpir.marketAnalysis << "\n";
    output << "- 竞品分析：" << result.cpir.competitorAnalysis << "\n";
    output << "- 财务预测：" << result.cpir.financialProjection << "\n";
    output << "- 团队结构：" << result.cpir.teamStructure << "\n";
    output << "- 当前成果：" << result.cpir.currentResults << "\n";
    output << "- 社会价值：" << result.cpir.socialValue << "\n";
    output << "- 缺失字段：" << joinStrings(result.cpir.missingFields) << "\n";
    output << "- 风险项：" << joinStrings(result.cpir.riskItems) << "\n\n";

    output << "## 材料一致性风险\n\n";
    for (const auto& issue : result.consistencyIssues) {
        output << "- " << issue.issueId << " [" << toString(issue.severity) << "] "
               << issue.description << "；影响文件：" << joinPaths(issue.affectedFiles)
               << "；建议：" << issue.fixSuggestion << "\n";
    }
    if (result.consistencyIssues.empty()) {
        output << "未发现材料一致性风险。\n";
    }

    output << "\n## 赛道规则审计\n\n";
    if (result.findings.empty()) {
        output << "未发现规则触发项。\n\n";
    }
    for (const auto& finding : result.findings) {
        output << "### " << finding.ruleId << " " << finding.title << "\n\n";
        output << "- 严重度：" << toString(finding.severity) << "\n";
        output << "- 原因：" << finding.reason << "\n";
        output << "- 证据：" << joinPaths(finding.evidence) << "\n";
        output << "- 缺失证据：" << joinStrings(finding.missingEvidence) << "\n";
        output << "- 修复建议：" << finding.fixSuggestion << "\n\n";
    }

    output << "## 可信评分\n\n";
    for (const auto& [dimension, score] : result.trustScore.dimensions) {
        output << "- " << dimension << "：" << score << "\n";
    }
    for (const auto& penalty : result.trustScore.penalties) {
        output << "- 扣分 " << penalty.points << "：[" << penalty.ruleId << "] "
               << penalty.dimension << "，" << penalty.reason << "\n";
    }
    output << "\n";

    output << "## 声明—证据匹配\n\n";
    for (const auto& claim : result.claims) {
        const auto iter = std::find_if(
            result.evidenceMatches.begin(), result.evidenceMatches.end(),
            [&](const EvidenceMatch& match) { return match.claimId == claim.claimId; });
        output << "- " << claim.claimId << " [" << toString(claim.claimType) << "] "
               << claim.claimText << "；来源：" << util::pathString(claim.sourceFile);
        if (iter != result.evidenceMatches.end()) {
            output << " -> " << toString(iter->status) << "（" << iter->reason << "）";
            if (!iter->evidenceFiles.empty()) {
                output << "；证据：" << joinPaths(iter->evidenceFiles);
            }
            if (!iter->missingEvidence.empty()) {
                output << "；缺失：" << joinStrings(iter->missingEvidence);
            }
        }
        output << "\n";
    }
    if (result.claims.empty()) {
        output << "未抽取到承诺性声明。\n";
    }

    output << "\n## 补证任务\n\n";
    for (const auto& task : result.fixTasks) {
        output << "- " << task.taskId << " [" << task.priority << "] " << task.title << "："
               << task.reason << "；材料：" << joinStrings(task.requiredMaterial) << "；规则："
               << joinStrings(task.affectedRules) << "\n";
    }
    if (result.fixTasks.empty()) {
        output << "暂无补证任务。\n";
    }

    output << "\n## 修复计划\n\n";
    output << result.repairPlan.markdown << "\n";

    output << "## 二次审计差分\n\n";
    if (diff == nullptr) {
        output << "当前报告未绑定旧版审计包；完成修复工作区二次审计后可生成差分。\n\n";
    } else {
        output << "- 可信评分：" << diff->oldScore << " -> " << diff->newScore << "\n";
        output << "- 可信债务：" << diff->oldTrustDebt << " -> " << diff->newTrustDebt << "\n";
        output << "- 阻断项：" << diff->oldBlockers << " -> " << diff->newBlockers << "\n";
        output << "- 警告项：" << diff->oldWarnings << " -> " << diff->newWarnings << "\n";
        output << "- 证据覆盖率：" << diff->oldEvidenceCoverage << "% -> "
               << diff->newEvidenceCoverage << "%\n";
        output << "- 补证任务：" << diff->oldFixTaskCount << " -> " << diff->newFixTaskCount
               << "\n\n";
        output << diff->summary << "\n\n";
    }
    output << "本报告不直接覆盖原项目，不生成虚假数据，关键结论均保留规则 ID 或证据来源。\n";
    return output.str();
}

Result<void> MarkdownReporter::write(const AuditResult& result, const std::filesystem::path& output,
                                     const AuditDiff* diff) const {
    return util::writeTextFile(output, render(result, diff));
}

} // namespace cc
