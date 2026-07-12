/**
 * @file AgentPermissionPolicy.cpp
 * @brief Agent 工具权限策略实现。
 */

#include "cc/agent/AgentPermissionPolicy.hpp"

#include "cc/agent/PermissionGate.hpp"

namespace cc {

Result<void> AgentPermissionPolicy::authorize(const AgentRunRequest& request,
                                              ToolPermission permission) const {
    PermissionGate gate;
    request.allowWriteWorkspace ? gate.allow(ToolPermission::WriteWorkspace)
                                : gate.deny(ToolPermission::WriteWorkspace);
    request.allowReadExternal ? gate.allow(ToolPermission::ReadExternalFiles)
                              : gate.deny(ToolPermission::ReadExternalFiles);
    request.allowModifyOriginal ? gate.allow(ToolPermission::ModifyOriginalProject)
                                : gate.deny(ToolPermission::ModifyOriginalProject);
    request.allowExecuteCommand ? gate.allow(ToolPermission::ExecuteCommand)
                                : gate.deny(ToolPermission::ExecuteCommand);
    request.allowNetwork ? gate.allow(ToolPermission::NetworkAccess)
                         : gate.deny(ToolPermission::NetworkAccess);
    request.allowLlm ? gate.allow(ToolPermission::LLMAccess) : gate.deny(ToolPermission::LLMAccess);
    if (!gate.isAllowed(permission)) {
        return Result<void>::failure("权限门控拒绝工具调用，需要权限: " + toString(permission));
    }
    return Result<void>::success();
}

} // namespace cc
