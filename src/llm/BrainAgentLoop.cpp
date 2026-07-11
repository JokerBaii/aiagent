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

[[nodiscard]] bool requestCancelled(const AgentRunRequest& request) {
    return request.isCancelled && request.isCancelled();
}

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
    return run(config, request, {}, maxSteps);
}

Result<AgentRunResult> BrainAgentLoop::run(const LlmConfig& config,
                                           const AgentRunRequest& request,
                                           AgentEventObserver observe,
                                           std::size_t maxSteps) const {
    LlmBrain brain;
    return runWithDecisionProvider(
        request,
        [&](const AgentRunRequest& activeRequest, const AgentRunResult& result,
            const std::vector<AgentToolSpec>& tools) {
            return brain.decideNextAgentStep(config, activeRequest, result, tools);
        },
        maxSteps, std::move(observe));
}

Result<AgentRunResult>
BrainAgentLoop::runWithDecisionProvider(const AgentRunRequest& request,
                                        AgentDecisionProvider decide,
                                        std::size_t maxSteps,
                                        AgentEventObserver observe) const {
    AgentRunResult result;
    result.plan.summary = "LLM Brain 迭代工具循环：每一步基于已有观察选择一个受控工具，"
                          "或在信息足够时给出最终回答。";
    const auto stepLimit = std::max<std::size_t>(1U, maxSteps);
    const auto tools = ToolRegistry{}.interactiveToolSpecs();
    AgentRuntime runtime;
    auto activeRequest = request;

    for (std::size_t step = 1U; step <= stepLimit; ++step) {
        if (requestCancelled(activeRequest)) {
            return Result<AgentRunResult>::failure("智能体任务已取消");
        }
        auto decision = decide(activeRequest, result, tools);
        if (!decision.ok()) {
            return Result<AgentRunResult>::failure(decision.error());
        }
        if (decision.value().kind == AgentDecisionKind::FinalAnswer) {
            if (activeRequest.requireAudit && activeRequest.auditResult == nullptr) {
                AgentObservation required{
                    .callId = "audit_required_" + std::to_string(step),
                    .toolName = "run_project_audit",
                    .ok = false,
                    .summary = "本轮目标要求先运行项目审计，不能在没有规则结果时直接回答",
                    .output = JsonValue::Object{{"required_tool", "run_project_audit"}}};
                result.observations.push_back(required);
                result.events.push_back(toolEvent(required));
                if (observe) {
                    observe(result.events.back());
                }
                result.trace = agentRunTraceJson(result);
                continue;
            }
            setAgentFinalAnswer(result, decision.value().finalAnswer, "Brain final answer");
            if (observe) {
                observe(result.events.back());
            }
            return Result<AgentRunResult>::success(std::move(result));
        }

        auto call = decision.value().call;
        if (call.id.empty()) {
            call.id = "brain_step_" + std::to_string(step);
            decision.value().call.id = call.id;
        }
        result.plan.calls.push_back(call);
        result.events.push_back(decisionEvent(decision.value(), step));
        if (observe) {
            observe(result.events.back());
        }

        if (call.name == "re_audit_repaired_project" && result.auditDiff.has_value()) {
            AgentObservation duplicate{
                .callId = call.id,
                .toolName = call.name,
                .ok = false,
                .summary = "本轮已完成一次原始基线到最终修复副本的二次审计；请先收束结果",
                .output = JsonValue::Object{{"reason", "re_audit_already_completed"}}};
            result.observations.push_back(duplicate);
            result.events.push_back(toolEvent(duplicate));
            if (observe) {
                observe(result.events.back());
            }
            result.trace = agentRunTraceJson(result);
            continue;
        }

        std::size_t streamedObservations = 0U;
        auto execution = runtime.runToolExecution(
            activeRequest, call,
            [&](const AgentObservation& observation) {
                ++streamedObservations;
                if (observe) {
                    observe(toolEvent(observation));
                }
            });
        if (requestCancelled(activeRequest)) {
            return Result<AgentRunResult>::failure("智能体任务已取消");
        }
        if (!execution.ok()) {
            return Result<AgentRunResult>::failure(execution.error());
        }
        std::size_t observationIndex = 0U;
        for (auto& observation : execution.value().observations) {
            result.observations.push_back(observation);
            result.events.push_back(toolEvent(observation));
            if (observe && observationIndex >= streamedObservations) {
                observe(result.events.back());
            }
            ++observationIndex;
        }
        if (execution.value().auditResult.has_value()) {
            result.auditResult = std::move(execution.value().auditResult);
            activeRequest.auditResult = &(*result.auditResult);
            if (activeRequest.baselineAuditResult == nullptr) {
                activeRequest.baselineAuditResult = activeRequest.auditResult;
            }
            activeRequest.projectRoot = result.auditResult->context.inputRoot;
            activeRequest.workspaceRoot = execution.value().auditDiff.has_value()
                                              ? activeRequest.workspaceRoot
                                              : result.auditResult->context.workspaceRoot / "agent";
            activeRequest.requireAudit = false;
        }
        if (execution.value().auditDiff.has_value()) {
            result.auditDiff = std::move(execution.value().auditDiff);
        }
        result.trace = agentRunTraceJson(result);
    }

    if (request.requireAudit && !result.auditResult.has_value()) {
        return Result<AgentRunResult>::failure(
            "Brain 在最大步数内没有调用必需的 run_project_audit 工具");
    }
    setAgentFinalAnswer(result, boundedFinalAnswer(result.observations, stepLimit),
                        "Brain step limit");
    if (observe) {
        observe(result.events.back());
    }
    return Result<AgentRunResult>::success(std::move(result));
}

} // namespace cc
