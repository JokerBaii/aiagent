/**
 * @file ReportAnalyzer.hpp
 * @brief 报告完整性确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查报告所需的项目概况和 CPIR 字段是否可导出。
 */
class ReportAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 报告完整性分析适用于所有赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查报告所需 CPIR 字段和风险提示是否充分。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
