/**
 * @file AdvisoryReconciler.cpp
 * @brief 把 LLM 风险研判与确定性审计结果对齐的校验器实现。
 */

#include "cc/llm/AdvisoryReconciler.hpp"

#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cc {
namespace {

[[nodiscard]] bool matchesFinding(const AdvisoryRiskItem& item, const AuditFinding& finding) {
    if (!item.ruleIdHint.empty()) {
        return util::lowerAscii(item.ruleIdHint) == util::lowerAscii(finding.ruleId);
    }
    if (!item.claimIdHint.empty()) {
        return false;
    }
    const auto title = util::lowerAscii(item.title);
    const auto findingTitle = util::lowerAscii(finding.title);
    return !title.empty() && title == findingTitle;
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
                match.status == EvidenceStatus::Conflicted ||
                match.status == EvidenceStatus::NeedReview);
    });
}

[[nodiscard]] bool containsAny(const std::string& text,
                               const std::vector<std::string_view>& phrases) {
    return std::any_of(phrases.begin(), phrases.end(), [&](std::string_view phrase) {
        return util::contains(text, phrase);
    });
}

[[nodiscard]] bool claimsPass(const AdvisoryRiskItem& item) {
    const auto text = util::lowerAscii(item.title + " " + item.reason);
    const std::vector<std::string_view> negative{
        "不通过", "不能通过", "未通过", "禁止通过", "并非无风险", "不是无风险",
        "存在风险", "存在问题", "发现问题", "不符合", "尚未满足", "缺少", "不足"};
    if (containsAny(text, negative)) {
        return false;
    }
    const std::vector<std::string_view> positive{
        "可以通过", "建议通过", "符合要求", "无风险", "没有问题", "不存在问题",
        "已满足"};
    return containsAny(text, positive);
}

[[nodiscard]] bool claimsWholeProjectPass(const AdvisoryRiskItem& item) {
    if (!claimsPass(item)) {
        return false;
    }
    const auto text = util::lowerAscii(item.title + " " + item.reason);
    const std::vector<std::string_view> scope{"整体", "整个项目", "项目总体", "最终结论",
                                               "审计结论", "提交", "参赛"};
    return containsAny(text, scope);
}

[[nodiscard]] std::string advisoryKey(const AdvisoryRiskItem& item) {
    return util::lowerAscii(item.ruleIdHint + "\n" + item.claimIdHint + "\n" + item.title);
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
    out.suggestedScore = std::clamp(advisory.suggestedScore, 0, 100);
    out.scoreGap = out.suggestedScore - result.trustScore.totalScore;

    const bool projectHasBlocker = hasBlocker(result.findings);

    std::set<std::string> seen;
    const auto riskLimit = std::min<std::size_t>(advisory.risks.size(), 100U);
    for (std::size_t index = 0U; index < riskLimit; ++index) {
        const auto& item = advisory.risks[index];
        if (!seen.insert(advisoryKey(item)).second) {
            continue;
        }
        ReconciledRiskItem reconciled;
        reconciled.advisory = item;

        const auto findingIter = std::find_if(
            result.findings.begin(), result.findings.end(),
            [&](const AuditFinding& finding) { return matchesFinding(item, finding); });

        if (findingIter != result.findings.end()) {
            if (findingIter->severity != item.severity || claimsPass(item)) {
                reconciled.verdict = AdvisoryVerdict::Conflicting;
                reconciled.reconciliation =
                    "与规则冲突：对应规则 " + findingIter->ruleId +
                    "，但风险严重度或通过结论与确定性结果不一致。";
            } else {
                reconciled.verdict = AdvisoryVerdict::Confirmed;
                reconciled.reconciliation = "已印证：对应规则 " + findingIter->ruleId + "（" +
                                            findingIter->title + "）。";
            }
        } else if (matchesEvidenceGap(item, result.evidenceMatches)) {
            if (claimsPass(item)) {
                reconciled.verdict = AdvisoryVerdict::Conflicting;
                reconciled.reconciliation =
                    "与证据冲突：声明 " + item.claimIdHint + " 仍有证据缺口。";
            } else {
                reconciled.verdict = AdvisoryVerdict::Confirmed;
                reconciled.reconciliation =
                    "已印证：对应声明 " + item.claimIdHint + " 的证据缺口。";
            }
        } else {
            reconciled.verdict = AdvisoryVerdict::Unverified;
            reconciled.reconciliation =
                "待核实：未找到对应规则风险或证据缺口，仅作参考，不影响评分。";
        }

        if (reconciled.verdict == AdvisoryVerdict::Unverified && projectHasBlocker &&
            claimsWholeProjectPass(item)) {
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
