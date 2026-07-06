/**
 * @file SocialPracticeAnalyzer.hpp
 * @brief 社会实践赛道确定性分析器。
 */

#pragma once

#include "cc/agent/IAnalyzer.hpp"

namespace cc {

/**
 * @brief 检查社会实践项目的过程记录和影响证据。
 */
class SocialPracticeAnalyzer : public IAnalyzer {
  public:
    /** @brief 返回分析器名称。 */
    [[nodiscard]] std::string name() const override;
    /** @brief 仅支持社会实践赛道。 */
    [[nodiscard]] bool supports(CompetitionType type) const override;
    /** @brief 检查社会价值字段和实践证明材料是否具备证据支撑。 */
    [[nodiscard]] Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const override;
};

} // namespace cc
