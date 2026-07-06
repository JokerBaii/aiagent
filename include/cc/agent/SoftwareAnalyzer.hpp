/**
 * @file SoftwareAnalyzer.hpp
 * @brief 软件赛道确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查软件项目源码、技术路线和复现入口是否完整。
 */
class SoftwareAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 仅支持软件项目赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查源码、技术路线和构建/部署入口是否可复核。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
