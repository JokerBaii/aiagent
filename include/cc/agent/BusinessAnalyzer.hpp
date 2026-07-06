/**
 * @file BusinessAnalyzer.hpp
 * @brief 商业赛道确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查商业闭环材料是否具备可复核基础。
 */
class BusinessAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 仅支持商业创新赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查商业计划、财务预测和商业模式字段是否形成闭环。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
