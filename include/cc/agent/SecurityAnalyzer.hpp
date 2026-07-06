/**
 * @file SecurityAnalyzer.hpp
 * @brief 安全边界确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查敏感文件和不可直接提交的安全风险。
 */
class SecurityAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 安全分析适用于所有赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查敏感文件和不应提交的安全风险材料。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
