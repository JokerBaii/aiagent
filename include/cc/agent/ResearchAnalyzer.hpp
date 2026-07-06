/**
 * @file ResearchAnalyzer.hpp
 * @brief 科研赛道确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查科研项目是否具备实验数据和研究结论证据。
 */
class ResearchAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 仅支持科研学术赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查实验数据和研究结果材料是否具备复核基础。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
