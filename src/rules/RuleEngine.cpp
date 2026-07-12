#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RuleConditionEvaluator.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {
namespace {

[[nodiscard]] bool appliesToTrack(std::string_view ruleTrack, CompetitionType type) {
    if (ruleTrack == "common") {
        return true;
    }
    if (ruleTrack == trackKey(type)) {
        return true;
    }
    if ((type == CompetitionType::AiApplication || type == CompetitionType::EngineeringProduct) &&
        ruleTrack == "software_project") {
        return true;
    }
    if (type == CompetitionType::PublicWelfare && ruleTrack == "social_practice") {
        return true;
    }
    return type == CompetitionType::ComprehensiveInnovation &&
           (ruleTrack == "business_innovation" || ruleTrack == "software_project");
}

} // namespace

std::vector<AuditFinding> RuleEngine::evaluate(const std::vector<AuditRule>& rules,
                                               const ProjectInventory& inventory, const CPIR& cpir,
                                               const std::vector<ProjectClaim>& claims,
                                               const std::vector<EvidenceMatch>& matches,
                                               const std::vector<ConsistencyIssue>& issues) const {
    std::vector<AuditFinding> findings;
    const RuleConditionEvaluator evaluator;
    for (const auto& rule : rules) {
        if (!appliesToTrack(rule.track, cpir.competitionType)) {
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
