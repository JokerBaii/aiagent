/**
 * @file AgentPermissionPolicy.hpp
 * @brief 将 Agent 请求能力快照转换为单次工具权限决策。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class AgentPermissionPolicy {
  public:
    /** @brief 权限满足时成功，否则返回可展示的拒绝原因。 */
    [[nodiscard]] Result<void> authorize(const AgentRunRequest& request,
                                         ToolPermission permission) const;
};

} // namespace cc
