/**
 * @file SocialPracticeAnalyzer.cpp
 * @brief 社会实践赛道确定性分析器实现。
 */

#include "cc/agent/SocialPracticeAnalyzer.hpp"
#include "cc/inventory/InventoryEngine.hpp"

namespace cc {

std::string SocialPracticeAnalyzer::name() const {
    return "SocialPracticeAnalyzer";
}

bool SocialPracticeAnalyzer::supports(CompetitionType type) const {
    return type == CompetitionType::SocialPractice;
}

Result<std::vector<AuditFinding>>
SocialPracticeAnalyzer::analyze(const CPIR& cpir, const ProjectInventory& inventory,
                                const std::vector<EvidenceMatch>&) const {
    std::vector<AuditFinding> findings;
    if (!supports(cpir.competitionType)) {
        return Result<std::vector<AuditFinding>>::success(findings);
    }
    if (cpir.socialValue.empty() || !hasRole(inventory, AssetRole::SocialPracticeProof)) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_SOCIAL_IMPACT";
        finding.severity = Severity::Warning;
        finding.title = "社会实践影响证据不足";
        finding.reason = "社会实践项目需要过程记录和影响数据支撑社会价值声明。";
        finding.missingEvidence = {"过程记录", "影响数据", "社会价值说明"};
        finding.fixSuggestion = "补充调研记录、活动证明、受益对象反馈或影响统计。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
