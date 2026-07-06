/**
 * @file HumanApprovalGate.cpp
 * @brief 高风险动作人工确认实现。
 */

#include "cc/agent/HumanApprovalGate.hpp"
#include "cc/agent/PermissionGate.hpp"

namespace cc {

bool HumanApprovalGate::approve(ToolPermission permission, bool userConfirmed) const {
    PermissionGate gate;
    return gate.isAllowed(permission) || userConfirmed;
}

} // namespace cc
