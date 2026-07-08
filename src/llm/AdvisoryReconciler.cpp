/**
 * @file AdvisoryReconciler.cpp
 * @brief 把 LLM 风险研判与确定性审计结果对齐的校验器实现。
 */

#include "cc/llm/AdvisoryReconciler.hpp"

#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <sstream>

namespace cc {
namespace {

[[nodiscard]] bool matchesFinding(const AdvisoryRiskItem& item, const AuditFinding& finding) {
    if (!item.ruleIdHint.empty() &&
        util::lowerAscii(item.ruleIdHint) == util::lowerAscii(finding.ruleId)) {
        return true;
    }
    // 无 ruleId 命中时，用标题/原因文本与规则标题做保守包含匹配，避免过度联想。
    const auto title = util::lowerAscii(item.title);
    const auto findingTitle = util::lowerAscii(finding.title);
    if (!title.empty() && !findingTitle.empty() &&
        (util::contains(findingTitle, title) || util::contains(title, findingTitle))) {
        return true;
    }
    return false;
}

[[nodiscard]] bool matchesEvidenceGap(const AdvisoryRiskItem& item,
                                      const std::vector<EvidenceMatch>& matches) {
    if (item.claimIdHint.empty()) {
        return false;
    }
    const auto hint = util::lowerAscii(item.claimIdHint);
    return std::any_of(matches.begin(), matches.end(), [&](const EvidenceMatch& match) {
        return util::lowerAscii(match.claimId) == hint &&
               (match.status == EvidenceStatus::Unsupported ||
                match.status == EvidenceStatus::Partial ||
                match.status == EvidenceStatus::Conflicted);
    });
}

[[nodiscard]] bool hasBlocker(const std::vector<AuditFinding>& findings) {
    return std::any_of(findings.begin(), findings.end(), [](const AuditFinding& finding) {
        return finding.severity == Severity::Blocker;
    });
}

} // namespace

std::string toString(AdvisoryVerdict verdict) {
    switch (verdict) {
    case AdvisoryVerdict::Confirmed:
        return "confirmed";
    case AdvisoryVerdict::Unverified:
        return "unverified";
    case AdvisoryVerdict::Conflicting:
        return "conflicting";
    }
    return "unverified";
}

ReconciledAdvisory AdvisoryReconciler::reconcile(const AuditAdvisory& advisory,
                                                 const AuditResult& result) const {
    ReconciledAdvisory out;
    out.finalScore = result.trustScore.totalScore;
    out.suggestedScore = advisory.suggestedScore;
    out.scoreGap = advisory.suggestedScore - result.trustScore.totalScore;

    const bool projectHasBlocker = hasBlocker(result.findings);

    for (const auto& item : advisory.risks) {
        ReconciledRiskItem reconciled;
        reconciled.advisory = item;

        const auto findingIter = std::find_if(
            result.findings.begin(), result.findings.end(),
            [&](const AuditFinding& finding) { return matchesFinding(item, finding); });

        if (findingIter != result.findings.end()) {
            reconciled.verdict = AdvisoryVerdict::Confirmed;
            reconciled.reconciliation =
                "已印证：对应规则 " + findingIter->ruleId + "（" + findingIter->title + "）。";
        } else if (matchesEvidenceGap(item, result.evidenceMatches)) {
            reconciled.verdict = AdvisoryVerdict::Confirmed;
            reconciled.reconciliation = "已印证：对应声明 " + item.claimIdHint + " 的证据缺口。";
        } else {
            reconciled.verdict = AdvisoryVerdict::Unverified;
            reconciled.reconciliation =
                "待核实：未找到对应规则风险或证据缺口，仅作参考，不影响评分。";
        }

        // 冲突降级：LLM 判为无风险/通过，但确定性结果存在 Blocker。
        const auto reason = util::lowerAscii(item.reason);
        const bool claimsPass = util::contains(reason, "通过") ||
                                util::contains(reason, "无风险") ||
                                util::contains(reason, "没有问题");
        if (claimsPass && projectHasBlocker) {
            reconciled.verdict = AdvisoryVerdict::Conflicting;
            reconciled.reconciliation =
                "与规则冲突：确定性审计存在必须处理项，已否决该乐观研判并降级。";
        }

        switch (reconciled.verdict) {
        case AdvisoryVerdict::Confirmed:
            ++out.confirmedCount;
            break;
        case AdvisoryVerdict::Unverified:
            ++out.unverifiedCount;
            break;
        case AdvisoryVerdict::Conflicting:
            ++out.conflictingCount;
            break;
        }
        out.items.push_back(std::move(reconciled));
    }

    std::ostringstream summary;
    summary << "确定性评分 " << out.finalScore << "/100";
    if (advisory.suggestedScore > 0) {
        summary << "，LLM 建议 " << advisory.suggestedScore << "（差 " << out.scoreGap << "）";
    }
    summary << "。研判印证 " << out.confirmedCount << " 条、待核实 " << out.unverifiedCount
            << " 条、冲突 " << out.conflictingCount << " 条。最终评分以确定性规则为准。";
    out.summary = summary.str();
    return out;
}

} // namespace cc
