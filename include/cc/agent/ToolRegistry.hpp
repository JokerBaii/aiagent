/**
 * @file ToolRegistry.hpp
 * @brief 内置工具注册信息。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"

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
};

} // namespace cc
