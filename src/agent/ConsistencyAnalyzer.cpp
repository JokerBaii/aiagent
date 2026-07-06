/**
 * @file ConsistencyAnalyzer.cpp
 * @brief CPIR 内部一致性确定性分析器实现。
 */

#include "cc/agent/ConsistencyAnalyzer.hpp"

namespace cc {

std::string ConsistencyAnalyzer::name() const {
    return "ConsistencyAnalyzer";
}

bool ConsistencyAnalyzer::supports(CompetitionType) const {
    return true;
}

Result<std::vector<AuditFinding>>
ConsistencyAnalyzer::analyze(const CPIR& cpir, const ProjectInventory&,
                             const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (!cpir.financialProjection.empty() && cpir.businessModel.empty()) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_CPIR_BUSINESS_CONSISTENCY";
        finding.severity = Severity::Warning;
        finding.title = "财务预测缺少商业模式承接";
        finding.reason = "财务预测需要和商业模式对应，否则答辩中收入逻辑不可复核。";
        finding.missingEvidence = {"商业模式"};
        finding.fixSuggestion = "补充商业模式，并说明财务预测与收入来源的对应关系。";
        findings.push_back(std::move(finding));
    }
    if (!cpir.marketAnalysis.empty() && cpir.targetUser.empty()) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_CPIR_MARKET_CONSISTENCY";
        finding.severity = Severity::Warning;
        finding.title = "市场分析缺少目标用户边界";
        finding.reason = "市场规模需要绑定目标用户，否则容易形成泛化市场声明。";
        finding.missingEvidence = {"目标用户"};
        finding.fixSuggestion = "补充目标用户画像和市场规模口径。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
