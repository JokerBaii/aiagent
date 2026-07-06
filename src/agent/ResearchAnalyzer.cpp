/**
 * @file ResearchAnalyzer.cpp
 * @brief 科研赛道确定性分析器实现。
 */

#include "cc/agent/ResearchAnalyzer.hpp"
#include "cc/inventory/InventoryEngine.hpp"

namespace cc {

std::string ResearchAnalyzer::name() const {
    return "ResearchAnalyzer";
}

bool ResearchAnalyzer::supports(CompetitionType type) const {
    return type == CompetitionType::ScientificResearch;
}

Result<std::vector<AuditFinding>>
ResearchAnalyzer::analyze(const CPIR& cpir, const ProjectInventory& inventory,
                          const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (!supports(cpir.competitionType)) {
        return Result<std::vector<AuditFinding>>::success(findings);
    }
    if (!hasRole(inventory, AssetRole::ExperimentData) || cpir.currentResults.empty()) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_RESEARCH_REPRODUCIBILITY";
        finding.severity = Severity::Warning;
        finding.title = "科研结果缺少复核材料";
        finding.reason = "科研项目需要实验数据、评价指标和研究结论互相支撑。";
        finding.missingEvidence = {"实验数据", "评价指标", "研究结论"};
        finding.fixSuggestion = "补充实验数据、指标口径和可追溯研究结论。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
