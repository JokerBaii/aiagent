/**
 * @file ConsistencyChecker.hpp
 * @brief 材料一致性风险检查。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief 材料一致性审计器。
 *
 * 本类检查 CPIR、资产和声明之间的矛盾，不直接修改材料，也不生成最终评分。
 */
class ConsistencyChecker {
  public:
    /**
     * @brief 检查项目材料之间的逻辑一致性。
     *
     * @param cpir 项目中间表示。
     * @param inventory 项目资产清单。
     * @param claims 已抽取声明。
     * @return 一致性问题列表；无问题时返回空列表。
     */
    [[nodiscard]] std::vector<ConsistencyIssue>
    check(const CPIR& cpir, const ProjectInventory& inventory,
          const std::vector<ProjectClaim>& claims) const;
};

} // namespace cc
