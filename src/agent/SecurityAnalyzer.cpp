/**
 * @file SecurityAnalyzer.cpp
 * @brief 安全边界确定性分析器实现。
 */

#include "cc/agent/SecurityAnalyzer.hpp"
#include "cc/inventory/InventoryEngine.hpp"

namespace cc {

std::string SecurityAnalyzer::name() const {
    return "SecurityAnalyzer";
}

bool SecurityAnalyzer::supports(CompetitionType) const {
    return true;
}

Result<std::vector<AuditFinding>>
SecurityAnalyzer::analyze(const CPIR&, const ProjectInventory& inventory,
                          const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (hasRole(inventory, AssetRole::SecretRisk)) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_SECURITY_SECRET";
        finding.severity = Severity::Blocker;
        finding.title = "材料包包含敏感文件";
        finding.reason = "包含密钥或环境配置会导致提交风险，修复必须只提示移除，不能读取或外传。";
        finding.evidence = filesWithRole(inventory, AssetRole::SecretRisk);
        finding.missingEvidence = {"移除敏感文件"};
        finding.fixSuggestion = "删除 .env、密钥或凭证文件，并提供脱敏配置模板。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
