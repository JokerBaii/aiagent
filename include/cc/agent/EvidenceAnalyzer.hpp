/**
 * @file EvidenceAnalyzer.hpp
 * @brief 证据覆盖确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 汇总未支撑声明，避免声明证据风险只散落在单条匹配里。
 */
class EvidenceAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 证据覆盖分析适用于所有赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查声明证据图中未支撑或待复核的声明。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
