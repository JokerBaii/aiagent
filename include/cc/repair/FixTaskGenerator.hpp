/**
 * @file FixTaskGenerator.hpp
 * @brief 补证任务生成。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief 补证任务生成器。
 *
 * 本类把规则风险和证据缺口转成可执行任务，不直接生成虚假材料。
 */
class FixTaskGenerator {
  public:
    /**
     * @brief 根据风险项和证据状态生成补证任务。
     */
    [[nodiscard]] std::vector<FixTask> generate(const std::vector<AuditFinding>& findings,
                                                const std::vector<EvidenceMatch>& matches) const;
};

} // namespace cc
