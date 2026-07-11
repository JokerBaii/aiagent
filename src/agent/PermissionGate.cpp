/**
 * @file PermissionGate.cpp
 * @brief 权限门控实现。
 */

#include "cc/agent/PermissionGate.hpp"

namespace cc {

PermissionGate::PermissionGate() {
    allowed_.insert(ToolPermission::ReadProjectFiles);
    allowed_.insert(ToolPermission::ExportReport);
}

bool PermissionGate::isAllowed(ToolPermission permission) const {
    return allowed_.find(permission) != allowed_.end();
}

void PermissionGate::allow(ToolPermission permission) {
    allowed_.insert(permission);
}

void PermissionGate::deny(ToolPermission permission) {
    allowed_.erase(permission);
}

std::vector<ToolPermission> PermissionGate::permissions() const {
    return {ToolPermission::ReadProjectFiles, ToolPermission::ReadExternalFiles,
            ToolPermission::WriteWorkspace,   ToolPermission::ModifyOriginalProject,
            ToolPermission::ExecuteCommand,   ToolPermission::NetworkAccess,
            ToolPermission::LLMAccess,        ToolPermission::ExportReport};
}

} // namespace cc
