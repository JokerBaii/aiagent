/**
 * @file RuleEngine.cpp
 * @brief 确定性规则执行实现。
 */

#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RuleConditionEvaluator.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

std::vector<AuditFinding> RuleEngine::evaluate(const std::vector<AuditRule>& rules,
                                               const ProjectInventory& inventory, const CPIR& cpir,
                                               const std::vector<ProjectClaim>& claims,
                                               const std::vector<EvidenceMatch>& matches,
                                               const std::vector<ConsistencyIssue>& issues) const {
    std::vector<AuditFinding> findings;
    const RuleConditionEvaluator evaluator;
    for (const auto& rule : rules) {
        if (rule.track != "common" && rule.track != trackKey(cpir.competitionType)) {
            continue;
        }

        const auto result = evaluator.evaluate(rule, inventory, cpir, claims, matches, issues);
        if (!result.failed) {
            continue;
        }
        AuditFinding finding;
        finding.ruleId = rule.ruleId;
        finding.severity = rule.severity;
        finding.title = rule.name + "失败";
        finding.reason = rule.failReason;
        if (!result.missing.empty()) {
            finding.reason += " 缺失/风险项: " + util::join(result.missing, "、");
        }
        finding.evidence = result.evidence;
        finding.missingEvidence = result.missing;
        finding.fixSuggestion = rule.fixTask;
        findings.push_back(std::move(finding));
    }
    return findings;
}

} // namespace cc
