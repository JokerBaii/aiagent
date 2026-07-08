/**
 * @file BrainAgentLoop.cpp
 * @brief LLM Brain 迭代工具循环实现。
 */

#include "cc/llm/BrainAgentLoop.hpp"

#include "cc/agent/AgentRuntime.hpp"
#include "cc/agent/ToolRegistry.hpp"
#include "cc/llm/LlmBrain.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace cc {
namespace {

[[nodiscard]] JsonValue callJson(const AgentToolCall& call) {
    return JsonValue::Object{
        {"id", call.id}, {"name", call.name}, {"reason", call.reason}, {"input", call.input}};
}

[[nodiscard]] JsonValue observationJson(const AgentObservation& observation) {
    return JsonValue::Object{{"call_id", observation.callId},
                             {"tool_name", observation.toolName},
                             {"ok", observation.ok},
                             {"summary", observation.summary},
                             {"output", observation.output}};
}

[[nodiscard]] AgentEvent decisionEvent(const AgentDecision& decision, std::size_t step) {
    return AgentEvent{.kind = AgentEventKind::Plan,
                      .role = "计划",
                      .text = decision.summary.empty() ? "Brain 已选择下一步工具调用"
                                                       : decision.summary,
                      .context = "LLM Brain step " + std::to_string(step),
                      .payload = JsonValue::Object{{"action", toString(decision.kind)},
                                                   {"call", callJson(decision.call)}}};
}

[[nodiscard]] AgentEvent toolEvent(const AgentObservation& observation) {
    return AgentEvent{.kind = AgentEventKind::Tool,
                      .role = "工具",
                      .text = observation.summary,
                      .context = observation.toolName,
                      .payload = observationJson(observation)};
}

[[nodiscard]] std::string boundedFinalAnswer(const std::vector<AgentObservation>& observations,
                                             std::size_t maxSteps) {
    std::ostringstream output;
    output << "Brain 已执行 " << maxSteps
           << " 轮受控工具调用，达到本轮最大步数，先基于已得到的观察结果收束。";
    for (const auto& observation : observations) {
        output << "\n- " << observation.summary;
    }
    return output.str();
}

} // namespace

Result<AgentRunResult> BrainAgentLoop::run(const LlmConfig& config, const AgentRunRequest& request,
                                           std::size_t maxSteps) const {
    AgentRunResult result;
    result.plan.summary = "LLM Brain 迭代工具循环：每一步基于已有观察选择一个受控工具，"
                          "或在信息足够时给出最终回答。";
    const auto stepLimit = std::max<std::size_t>(1U, maxSteps);
    const auto tools = ToolRegistry{}.interactiveToolSpecs();
    AgentRuntime runtime;
    LlmBrain brain;

    for (std::size_t step = 1U; step <= stepLimit; ++step) {
        auto decision = brain.decideNextAgentStep(config, request, result, tools);
        if (!decision.ok()) {
            return Result<AgentRunResult>::failure(decision.error());
        }
        if (decision.value().kind == AgentDecisionKind::FinalAnswer) {
            setAgentFinalAnswer(result, decision.value().finalAnswer, "Brain final answer");
            return Result<AgentRunResult>::success(std::move(result));
        }

        auto call = decision.value().call;
        if (call.id.empty()) {
            call.id = "brain_step_" + std::to_string(step);
            decision.value().call.id = call.id;
        }
        result.plan.calls.push_back(call);
        result.events.push_back(decisionEvent(decision.value(), step));

        auto observation = runtime.runTool(request, call);
        if (!observation.ok()) {
            return Result<AgentRunResult>::failure(observation.error());
        }
        result.observations.push_back(observation.value());
        result.events.push_back(toolEvent(observation.value()));
        result.trace = agentRunTraceJson(result);
    }

    setAgentFinalAnswer(result, boundedFinalAnswer(result.observations, stepLimit),
                        "Brain step limit");
    return Result<AgentRunResult>::success(std::move(result));
}

} // namespace cc
