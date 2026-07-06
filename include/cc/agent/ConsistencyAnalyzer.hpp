/**
 * @file ConsistencyAnalyzer.hpp
 * @brief CPIR 内部一致性确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查 CPIR 中商业、市场和技术字段是否存在基础矛盾。
 */
class ConsistencyAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 一致性分析适用于所有赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查 CPIR 内部字段是否存在基础矛盾。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
