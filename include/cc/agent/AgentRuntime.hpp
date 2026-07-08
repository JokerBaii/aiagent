/**
 * @file AgentRuntime.hpp
 * @brief 受控智能体运行时。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 执行 Brain 计划的受控运行时。
 *
 * AgentRuntime 不提供万能 shell。它只执行 ToolRegistry 声明的工具，并在每次调用前
 * 检查权限和路径边界。
 */
class AgentRuntime {
  public:
    /** @brief 在未授权 LLM 时生成并执行保守的本地计划。 */
    [[nodiscard]] Result<AgentRunResult> runLocal(const AgentRunRequest& request) const;
    /** @brief 执行单个工具调用。 */
    [[nodiscard]] Result<AgentObservation> runTool(const AgentRunRequest& request,
                                                   const AgentToolCall& call) const;
};

/** @brief 返回智能体 composer 支持的命令帮助文本。 */
[[nodiscard]] std::string agentCommandHelpText();
/** @brief 将智能体回合转换为可复核 JSON trace。 */
[[nodiscard]] JsonValue agentRunTraceJson(const AgentRunResult& result);
/** @brief 用 Brain 生成的最终回答更新回合结果和 assistant 事件。 */
void setAgentFinalAnswer(AgentRunResult& result, std::string finalAnswer, std::string context);

} // namespace cc
