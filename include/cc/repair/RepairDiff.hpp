/**
 * @file RepairDiff.hpp
 * @brief diff-first 修复产物生成。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief diff-first 修复产物生成器。
 *
 * 该类生成可审阅 diff 文本，确保修复建议先进入工作区和报告，而不是直接写回原项目。
 */
class RepairDiff {
  public:
    /**
     * @brief 根据补证任务生成建议性 diff。
     */
    [[nodiscard]] std::string generate(const std::vector<FixTask>& tasks, const CPIR& cpir) const;
};

} // namespace cc
