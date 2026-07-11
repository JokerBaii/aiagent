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
 * 默认只允许读取隔离项目和导出报告。写工作区、联网、LLM、执行命令、读取外部文件和
 * 覆盖原项目都必须由本次任务的权限快照显式允许。
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
