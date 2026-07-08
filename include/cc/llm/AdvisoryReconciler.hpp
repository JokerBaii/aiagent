/**
 * @file AdvisoryReconciler.hpp
 * @brief 把 LLM 风险研判与确定性审计结果对齐的校验器。
 *
 * AdvisoryReconciler 不调用 LLM，也不修改评分。它把 LLM 产出的 AuditAdvisory 与 AuditResult
 * 中的 RuleEngine findings、证据状态逐条比对：能对应上规则风险或证据缺口的判为已印证；找不到
 * 确定性依据的判为待核实；与确定性结论矛盾的判为冲突并降级。最终评分恒取 AuditResult 的
 * 确定性评分，实现“LLM 先判断、规则校验、冲突降级”的混合模式。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/llm/AuditAdvisory.hpp"

namespace cc {

class AdvisoryReconciler {
  public:
    /**
     * @brief 用确定性审计结果校验 LLM 研判。
     *
     * @param advisory LLM 产出的风险研判与评分建议。
     * @param result   确定性审计结果（规则、证据、评分的唯一裁决源）。
     * @return 校验后的结论；finalScore 恒为 result.trustScore.totalScore。
     */
    [[nodiscard]] ReconciledAdvisory reconcile(const AuditAdvisory& advisory,
                                               const AuditResult& result) const;
};

} // namespace cc
