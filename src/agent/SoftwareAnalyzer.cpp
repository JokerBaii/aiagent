/**
 * @file SoftwareAnalyzer.cpp
 * @brief 软件赛道确定性分析器实现。
 */

#include "cc/agent/SoftwareAnalyzer.hpp"
#include "cc/inventory/InventoryEngine.hpp"

namespace cc {

std::string SoftwareAnalyzer::name() const {
    return "SoftwareAnalyzer";
}

bool SoftwareAnalyzer::supports(CompetitionType type) const {
    return type == CompetitionType::SoftwareProject;
}

Result<std::vector<AuditFinding>>
SoftwareAnalyzer::analyze(const CPIR& cpir, const ProjectInventory& inventory,
                          const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (!supports(cpir.competitionType)) {
        return Result<std::vector<AuditFinding>>::success(findings);
    }
    if (cpir.technicalRoute.empty() || !hasRole(inventory, AssetRole::SourceCode) ||
        (!hasRole(inventory, AssetRole::BuildSystem) &&
         !hasRole(inventory, AssetRole::DependencyManifest))) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_SOFTWARE_REPRODUCIBILITY";
        finding.severity = Severity::Blocker;
        finding.title = "软件复现入口不足";
        finding.reason = "软件赛道需要技术路线、源码和构建或依赖入口支撑答辩复现。";
        finding.missingEvidence = {"技术路线", "源码", "构建入口或依赖清单"};
        finding.fixSuggestion = "补充 README 构建步骤、依赖清单和可运行源码入口。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
