#include "cc/audit/TrustScoreCalculator.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <set>

namespace cc {
namespace {

[[nodiscard]] int severityPenalty(Severity severity) {
    constexpr int kBlockerPenalty = 10;
    constexpr int kWarningPenalty = 5;
    constexpr int kInfoPenalty = 2;
    if (severity == Severity::Blocker) {
        return kBlockerPenalty;
    }
    return severity == Severity::Warning ? kWarningPenalty : kInfoPenalty;
}

[[nodiscard]] std::string dimensionForFinding(const AuditFinding& finding) {
    const auto id = util::lowerAscii(finding.ruleId);
    if (util::contains(id, "secret")) {
        return "成果真实性";
    }
    if (util::contains(id, "generated") || util::contains(id, "vendor")) {
        return "技术/商业/科研可行性";
    }
    if (id.starts_with("evidence_")) {
        return "声明-证据匹配度";
    }
    if (id.starts_with("biz_") || id.starts_with("soft_") || id.starts_with("res_") ||
        id.starts_with("soc_") || id.starts_with("ecom_") || id.starts_with("ai_") ||
        id.starts_with("eng_")) {
        return "赛道规则匹配度";
    }
    if (util::contains(id, "consistency")) {
        return "项目逻辑自洽性";
    }
    return "材料完整性";
}

[[nodiscard]] int evidencePenalty(EvidenceStatus status) {
    switch (status) {
    case EvidenceStatus::Supported:
        return 0;
    case EvidenceStatus::Partial:
        return 2;
    case EvidenceStatus::Unsupported:
        return 4;
    case EvidenceStatus::Conflicted:
        return 5;
    case EvidenceStatus::NeedReview:
        return 2;
    }
    return 0;
}

} // namespace

TrustScore TrustScoreCalculator::calculate(const ProjectInventory& inventory,
                                           const std::vector<AuditFinding>& findings,
                                           const std::vector<EvidenceMatch>& matches,
                                           const std::vector<ConsistencyIssue>& issues) const {
    TrustScore score;
    score.dimensions = {{"材料完整性", 15},           {"项目逻辑自洽性", 15},
                        {"声明-证据匹配度", 20},      {"赛道规则匹配度", 15},
                        {"技术/商业/科研可行性", 15}, {"成果真实性", 10},
                        {"答辩风险控制", 10}};

    std::set<std::string> appliedPenaltyIds;
    auto apply = [&](std::string ruleId, int points, std::string dimension, std::string reason) {
        if (points <= 0 || !appliedPenaltyIds.insert(ruleId).second) {
            return;
        }
        auto& dimensionScore = score.dimensions[dimension];
        const auto actual = std::min(points, dimensionScore);
        if (actual <= 0) {
            return;
        }
        dimensionScore -= actual;
        score.penalties.push_back(
            {std::move(ruleId), actual, std::move(dimension), std::move(reason)});
    };

    for (const auto& finding : findings) {
        if (finding.ruleId == "COMMON_CONSISTENCY_001") {
            continue;
        }
        apply(finding.ruleId, severityPenalty(finding.severity), dimensionForFinding(finding),
              finding.reason);
    }
    for (const auto& match : matches) {
        const auto reason = match.reason.empty() ? "声明证据尚未达到可独立复核标准" : match.reason;
        apply("EVIDENCE_" + match.claimId, evidencePenalty(match.status), "声明-证据匹配度",
              reason);
    }
    for (const auto& issue : issues) {
        apply(issue.issueId, severityPenalty(issue.severity), "项目逻辑自洽性", issue.description);
    }
    const auto generatedFinding =
        std::any_of(findings.begin(), findings.end(), [](const AuditFinding& finding) {
            return finding.ruleId == "COMMON_GENERATED_RATIO_001";
        });
    if (!generatedFinding && !inventory.assets.empty()) {
        const auto risky =
            countRole(inventory, AssetRole::Generated) + countRole(inventory, AssetRole::Vendored);
        const auto ratio =
            static_cast<double>(risky) / static_cast<double>(inventory.assets.size());
        if (ratio > 0.4) {
            apply("COMMON_GENERATED_RATIO_001", 5, "技术/商业/科研可行性",
                  "生成物或第三方依赖占比过高，会削弱自主贡献可信度。");
        }
    }

    int total = 0;
    for (const auto& [_, value] : score.dimensions) {
        total += value;
    }
    score.totalScore = std::clamp(total, 0, 100);
    score.trustDebt = 100 - score.totalScore;
    return score;
}

} // namespace cc
