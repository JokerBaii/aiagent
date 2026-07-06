/**
 * @file TrustScoreCalculator.cpp
 * @brief 可信评分计算实现。
 */

#include "cc/audit/TrustScoreCalculator.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>

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
    if (util::contains(id, "market") || util::contains(id, "evidence")) {
        return "声明-证据匹配度";
    }
    if (util::contains(id, "biz") || util::contains(id, "soft") || util::contains(id, "res")) {
        return "赛道规则匹配度";
    }
    return "材料完整性";
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

    auto apply = [&](std::string ruleId, int points, std::string dimension, std::string reason) {
        auto& dimensionScore = score.dimensions[dimension];
        const auto actual = std::min(points, dimensionScore);
        dimensionScore -= actual;
        score.penalties.push_back(
            {std::move(ruleId), actual, std::move(dimension), std::move(reason)});
    };

    for (const auto& finding : findings) {
        apply(finding.ruleId, severityPenalty(finding.severity), dimensionForFinding(finding),
              finding.reason);
    }
    for (const auto& match : matches) {
        if (match.status == EvidenceStatus::Unsupported) {
            apply("EVIDENCE_" + match.claimId, 4, "声明-证据匹配度", match.reason);
        } else if (match.status == EvidenceStatus::Partial) {
            apply("EVIDENCE_" + match.claimId, 2, "声明-证据匹配度", match.reason);
        }
    }
    for (const auto& issue : issues) {
        apply(issue.issueId, 3, "项目逻辑自洽性", issue.description);
    }
    if (!inventory.assets.empty()) {
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
