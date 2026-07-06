/**
 * @file HumanApprovalGate.hpp
 * @brief 高风险动作人工确认。
 */

#pragma once

#include "cc/core/Enums.hpp"

namespace cc {

/**
 * @brief 人工确认门。
 *
 * 高风险动作只有在默认权限允许或用户明确确认时才可继续。
 */
class HumanApprovalGate {
  public:
    /** @brief 根据权限和用户确认返回是否允许动作。 */
    [[nodiscard]] bool approve(ToolPermission permission, bool userConfirmed) const;
};

} // namespace cc
