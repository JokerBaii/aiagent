/**
 * @file IAnalyzer.hpp
 * @brief 专用分析器接口。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class IAnalyzer {
  public:
    virtual ~IAnalyzer() = default;
    /** @brief 返回分析器名称，用于会话记录和调试输出。 */
    [[nodiscard]] virtual std::string name() const = 0;
    /** @brief 判断分析器是否支持当前竞赛类型。 */
    [[nodiscard]] virtual bool supports(CompetitionType type) const = 0;
    /**
     * @brief 执行确定性专项分析。
     *
     * @return 成功时返回分析器发现的风险项；失败时返回分析器内部错误。
     */
    [[nodiscard]] virtual Result<std::vector<AuditFinding>>
    analyze(const CPIR& cpir, const ProjectInventory& inventory,
            const std::vector<EvidenceMatch>& evidenceGraph) const = 0;
};

} // namespace cc
