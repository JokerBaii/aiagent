/**
 * @file ITool.hpp
 * @brief Agentic Runtime 工具接口。
 */

#pragma once

#include "cc/core/Enums.hpp"
#include "cc/core/Result.hpp"

#include <filesystem>
#include <string>

namespace cc {

/**
 * @brief 工具输入。
 */
struct ToolInput {
    std::filesystem::path projectPath;
};

/**
 * @brief 工具执行结果。
 */
struct ToolResult {
    bool ok{false};
    std::string message;
};

/**
 * @brief 受控 Agent Runtime 工具接口。
 *
 * 工具必须声明所需权限，避免高风险动作绕过 PermissionGate。
 */
class ITool {
  public:
    virtual ~ITool() = default;
    /** @brief 返回工具名称。 */
    [[nodiscard]] virtual std::string name() const = 0;
    /** @brief 返回工具说明。 */
    [[nodiscard]] virtual std::string description() const = 0;
    /** @brief 返回运行工具所需权限。 */
    [[nodiscard]] virtual ToolPermission requiredPermission() const = 0;
    /** @brief 执行工具。 */
    [[nodiscard]] virtual Result<ToolResult> run(const ToolInput& input) const = 0;
};

} // namespace cc
