/**
 * @file RuleEngine.hpp
 * @brief 确定性规则执行。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief JSON 规则执行器。
 *
 * RuleEngine 只执行规则包中的确定性条件，不依赖 LLM，也不直接计算可信评分。
 */
class RuleEngine {
  public:
    /**
     * @brief 执行规则并生成风险项。
     *
     * @return 每条命中规则对应一个 AuditFinding，finding 必须保留 rule_id。
     */
    [[nodiscard]] std::vector<AuditFinding>
    evaluate(const std::vector<AuditRule>& rules, const ProjectInventory& inventory,
             const CPIR& cpir, const std::vector<ProjectClaim>& claims,
             const std::vector<EvidenceMatch>& matches,
             const std::vector<ConsistencyIssue>& issues) const;
};

} // namespace cc
