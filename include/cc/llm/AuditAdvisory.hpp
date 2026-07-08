/**
 * @file AuditAdvisory.hpp
 * @brief 混合审计研判模型：LLM 先给出风险研判与评分建议，确定性规则再校验。
 *
 * 该模型是“LLM 先判断、规则校验”混合模式的数据载体。LLM 产出 AuditAdvisory（风险研判 +
 * 建议评分 + 理由），随后确定性 AdvisoryReconciler 把每条研判与 RuleEngine 结果、证据状态
 * 逐条比对，标记为已印证 / 待核实 / 与规则冲突。最终可信评分仍由 TrustScoreCalculator 决定，
 * LLM 的建议评分只作为参考并接受降级。
 */

#pragma once

#include "cc/core/Enums.hpp"

#include <string>
#include <vector>

namespace cc {

/**
 * @brief LLM 提出的单条风险研判。
 *
 * ruleIdHint / claimIdHint 是 LLM 认为该研判对应的规则或声明，用于后续与确定性结果对齐；
 * 允许为空，为空时该研判默认落入“待核实”。
 */
struct AdvisoryRiskItem {
    std::string title;
    Severity severity{Severity::Warning};
    std::string reason;
    std::string ruleIdHint;
    std::string claimIdHint;
    std::string suggestion;
};

/**
 * @brief LLM 对整个项目的研判结论。
 *
 * suggestedScore 是 LLM 的主观评分建议，不作为最终评分，只用于和确定性评分对比并解释差异。
 */
struct AuditAdvisory {
    std::vector<AdvisoryRiskItem> risks;
    int suggestedScore{0};
    std::string overallJudgement;
};

/**
 * @brief 单条研判经确定性校验后的核对状态。
 */
enum class AdvisoryVerdict {
    Confirmed,  ///< 与某条规则风险或证据缺口一致，已印证。
    Unverified, ///< 找不到确定性依据，仅作参考，不影响评分。
    Conflicting ///< 与确定性结果矛盾（如 LLM 判为通过但规则判为 Blocker），降级并标注。
};

/**
 * @brief 校验后的单条研判。
 */
struct ReconciledRiskItem {
    AdvisoryRiskItem advisory;
    AdvisoryVerdict verdict{AdvisoryVerdict::Unverified};
    std::string reconciliation; ///< 校验说明：命中了哪条规则/证据，或为何判为待核实/冲突。
};

/**
 * @brief 研判与确定性审计结果对齐后的完整结论。
 *
 * finalScore 恒等于 TrustScoreCalculator 的确定性评分；scoreGap 记录 LLM 建议评分与确定性
 * 评分的差异，供用户判断 LLM 是否过于乐观/悲观。
 */
struct ReconciledAdvisory {
    std::vector<ReconciledRiskItem> items;
    int finalScore{0};
    int suggestedScore{0};
    int scoreGap{0};
    std::size_t confirmedCount{0};
    std::size_t unverifiedCount{0};
    std::size_t conflictingCount{0};
    std::string summary;
};

/** @brief 将研判核对状态转为稳定字符串。 */
[[nodiscard]] std::string toString(AdvisoryVerdict verdict);

} // namespace cc
