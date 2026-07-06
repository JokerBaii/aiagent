/**
 * @file ReportAnalyzer.cpp
 * @brief 报告完整性确定性分析器实现。
 */

#include "cc/agent/ReportAnalyzer.hpp"

namespace cc {

std::string ReportAnalyzer::name() const {
    return "ReportAnalyzer";
}

bool ReportAnalyzer::supports(CompetitionType) const {
    return true;
}

Result<std::vector<AuditFinding>> ReportAnalyzer::analyze(const CPIR& cpir, const ProjectInventory&,
                                                          const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (cpir.projectName.empty() || !cpir.missingFields.empty()) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_REPORT_COMPLETENESS";
        finding.severity = Severity::Warning;
        finding.title = "报告关键字段不完整";
        finding.reason = "中文可信报告必须包含项目概况和 CPIR 关键字段，缺失字段需要显式列出。";
        finding.missingEvidence = cpir.missingFields;
        if (cpir.projectName.empty()) {
            finding.missingEvidence.push_back("项目名称");
        }
        finding.fixSuggestion = "补充项目名称、目标用户、解决方案、赛道相关关键字段。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
