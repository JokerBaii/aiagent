/**
 * @file ToolRegistry.hpp
 * @brief 内置工具注册信息。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/core/Result.hpp"

#include <string>
#include <vector>

namespace cc {

/**
 * @brief 内置审计工具注册表。
 *
 * 注册表固定列出受控审计流水线可用工具，避免系统退化为自由执行命令的通用
 * agent。
 */
class ToolRegistry {
  public:
    /** @brief 返回当前对话式 Brain 可以直接驱动的工具说明。 */
    [[nodiscard]] std::vector<AgentToolSpec> interactiveToolSpecs() const;

    /**
     * @brief 按注册 schema 校验一次工具输入。
     *
     * 校验会拒绝未知字段、类型不匹配、非有限数值和超出资源上限的参数。运行时在任何
     * 数值转为 size_t 之前调用该方法，避免模型输入触发溢出或超量分配。
     */
    [[nodiscard]] Result<void> validateInteractiveInput(const std::string& toolName,
                                                        const JsonValue& input) const;
};

} // namespace cc
