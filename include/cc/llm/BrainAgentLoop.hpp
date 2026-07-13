/**
 * @file BrainAgentLoop.hpp
 * @brief LLM Brain 驱动的迭代工具循环。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/llm/LlmTypes.hpp"

#include <cstddef>
#include <functional>

namespace cc {

using AgentDecisionProvider = std::function<Result<AgentDecision>(
    const AgentRunRequest&, const AgentRunResult&, const std::vector<AgentToolSpec>&)>;
using AgentEventObserver = std::function<void(const AgentEvent&)>;

/**
 * @brief 让 LLM Brain 像 Codex/Claude Code 一样逐步选择工具并根据观察继续决策。
 *
 * BrainAgentLoop 只负责编排：大模型输出下一步决策，AgentRuntime 执行注册工具并记录
 * observation。它不提供自由 shell，也不改写最终评分。
 */
class BrainAgentLoop {
  public:
    /** @brief 运行一轮 Brain 工具循环。 */
    [[nodiscard]] Result<AgentRunResult>
    run(const LlmConfig& config, const AgentRunRequest& request, std::size_t maxSteps = 0U) const;
    /** @brief 运行工具循环并在每个事件产生时通知观察者。 */
    [[nodiscard]] Result<AgentRunResult> run(const LlmConfig& config,
                                             const AgentRunRequest& request,
                                             AgentEventObserver observe,
                                             std::size_t maxSteps = 0U) const;

    /** @brief 使用注入的决策器运行同一工具循环，供确定性集成测试和替代模型传输使用。 */
    [[nodiscard]] Result<AgentRunResult>
    runWithDecisionProvider(const AgentRunRequest& request, AgentDecisionProvider decide,
                            std::size_t maxSteps = 32U, AgentEventObserver observe = {}) const;
};

} // namespace cc
