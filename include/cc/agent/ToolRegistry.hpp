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
 * 注册表固定列出审计流水线和显式权限模式可用工具；高风险文件写入与 Shell 工具
 * 仍由每次请求的权限快照门控。
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
