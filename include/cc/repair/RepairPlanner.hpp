/**
 * @file RepairPlanner.hpp
 * @brief 修复计划生成。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief 修复计划生成器。
 *
 * RepairPlanner 只生成补证计划和 diff-first 产物，不覆盖原始项目。
 */
class RepairPlanner {
  public:
    /**
     * @brief 将补证任务组织为 Markdown 修复计划。
     */
    [[nodiscard]] RepairPlan plan(const std::vector<FixTask>& tasks, const CPIR& cpir) const;
};

} // namespace cc
