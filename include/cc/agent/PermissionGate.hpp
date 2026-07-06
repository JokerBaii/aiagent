/**
 * @file PermissionGate.hpp
 * @brief 权限门控。
 */

#pragma once

#include "cc/core/Enums.hpp"

#include <set>
#include <vector>

namespace cc {

/**
 * @brief 高风险能力权限门控。
 *
 * 默认只允许读项目、写工作区和导出报告；联网、LLM、执行命令和覆盖原项目默认拒绝。
 */
class PermissionGate {
  public:
    PermissionGate();
    /** @brief 判断权限当前是否允许。 */
    [[nodiscard]] bool isAllowed(ToolPermission permission) const;
    /** @brief 显式允许某项权限。 */
    void allow(ToolPermission permission);
    /** @brief 显式拒绝某项权限。 */
    void deny(ToolPermission permission);
    /** @brief 返回权限列表，供 Workbench 展示默认安全策略。 */
    [[nodiscard]] std::vector<ToolPermission> permissions() const;

  private:
    std::set<ToolPermission> allowed_;
};

} // namespace cc
