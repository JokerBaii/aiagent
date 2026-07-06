/**
 * @file ExternalToolAdapter.hpp
 * @brief 外部工具适配器登记与权限检查。
 *
 * 第一版不实际调用搜索、OCR、GitHub 或浏览器，只登记这些能力并通过 PermissionGate
 * 判断是否具备执行前置条件，避免默认联网或默认执行外部工具。
 */

#pragma once

#include "cc/core/Enums.hpp"

#include <string>
#include <vector>

namespace cc {

/**
 * @brief 外部工具预留能力描述。
 */
struct ExternalToolCapability {
    std::string name;
    ToolPermission permission{ToolPermission::ExecuteCommand};
    std::string description;
};

/**
 * @brief 外部工具适配器。
 *
 * 第一版只登记能力并做权限判断，防止系统默认联网、OCR 或访问浏览器/GitHub。
 */
class ExternalToolAdapter {
  public:
    /** @brief 返回预留的外部工具能力清单。 */
    [[nodiscard]] std::vector<ExternalToolCapability> reservedCapabilities() const;
    /** @brief 根据权限和用户确认判断能力是否可用。 */
    [[nodiscard]] bool canUse(const ExternalToolCapability& capability, bool userConfirmed) const;
};

} // namespace cc
