/**
 * @file BusinessAnalyzer.cpp
 * @brief 商业赛道确定性分析器实现。
 */

#include "cc/agent/BusinessAnalyzer.hpp"
#include "cc/inventory/InventoryEngine.hpp"

namespace cc {

std::string BusinessAnalyzer::name() const {
    return "BusinessAnalyzer";
}

bool BusinessAnalyzer::supports(CompetitionType type) const {
    return type == CompetitionType::BusinessInnovation || type == CompetitionType::Ecommerce;
}

Result<std::vector<AuditFinding>>
BusinessAnalyzer::analyze(const CPIR& cpir, const ProjectInventory& inventory,
                          const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (!supports(cpir.competitionType)) {
        return Result<std::vector<AuditFinding>>::success(findings);
    }
    if (cpir.businessModel.empty() || !hasRole(inventory, AssetRole::BusinessPlan) ||
        !hasRole(inventory, AssetRole::FinancialPlan)) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_BUSINESS_COMPLETENESS";
        finding.severity = Severity::Warning;
        finding.title = "商业闭环材料不完整";
        finding.reason = "商业赛道需要商业模式、商业计划书和财务预测互相支撑。";
        finding.missingEvidence = {"商业模式", "商业计划书", "财务预测"};
        finding.fixSuggestion = "补充商业模式、商业计划书和财务预测之间的证据链。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
