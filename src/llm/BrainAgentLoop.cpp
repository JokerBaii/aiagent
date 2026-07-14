/**
 * @file BrainAgentLoop.cpp
 * @brief LLM Brain 迭代工具循环实现。
 */

#include "cc/llm/BrainAgentLoop.hpp"

#include "cc/agent/AgentPermissionPolicy.hpp"
#include "cc/agent/AgentRuntime.hpp"
#include "cc/agent/AgentTraceSerializer.hpp"
#include "cc/agent/ToolRegistry.hpp"
#include "cc/llm/LlmBrain.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace cc {
namespace {

[[nodiscard]] bool requestCancelled(const AgentRunRequest& request) {
    if (!request.isCancelled) {
        return false;
    }
    try {
        return request.isCancelled();
    } catch (...) {
        return true;
    }
}

[[nodiscard]] bool retryableDecisionFailure(const std::string& error) {
    for (const auto marker : {"取消", "401", "403", "API key", "provider", "endpoint",
                              "未启用 DeepSeek", "请求预算", "最大工具步数"}) {
        if (error.find(marker) != std::string::npos) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] AgentEvent decisionEvent(const AgentDecision& decision, std::size_t step) {
    return AgentEvent{.kind = AgentEventKind::Plan,
                      .role = "计划",
                      .text = decision.summary.empty() ? "DeepSeek 已选择下一步工具调用"
                                                       : decision.summary,
                      .context = "DeepSeek step " + std::to_string(step),
                      .payload = JsonValue::Object{{"action", toString(decision.kind)},
                                                   {"call", agentToolCallJson(decision.call)}}};
}

[[nodiscard]] AgentEvent toolEvent(const AgentObservation& observation) {
    return AgentEvent{.kind = AgentEventKind::Tool,
                      .role = "工具",
                      .text = observation.summary,
                      .context = observation.toolName,
                      .payload = agentObservationJson(observation)};
}

[[nodiscard]] bool hasSuccessfulWorkspaceChange(const AgentRunResult& result) {
    return std::any_of(result.observations.begin(), result.observations.end(),
                       [](const AgentObservation& observation) {
                           return observation.ok &&
                                  (observation.toolName == "apply_repaired_text_edit" ||
                                   observation.toolName == "create_repaired_text_file");
                       });
}

[[nodiscard]] std::vector<AgentToolSpec> availableTools(const AgentRunRequest& request) {
    std::vector<AgentToolSpec> available;
    for (auto& tool : ToolRegistry{}.interactiveToolSpecs()) {
        if (AgentPermissionPolicy{}.authorize(request, tool.permission).ok()) {
            available.push_back(std::move(tool));
        }
    }
    return available;
}

[[nodiscard]] bool hasSuccessfulProjectRead(const AgentRunResult& result) {
    return std::any_of(result.observations.begin(), result.observations.end(),
                       [](const AgentObservation& observation) {
                           return observation.ok &&
                                  (observation.toolName == "read_text_file" ||
                                   observation.toolName == "read_extracted_document" ||
                                   observation.toolName == "read_repaired_text_file");
                       });
}

[[nodiscard]] std::string incompleteWorkflowReason(const AgentRunRequest& request,
                                                   const AgentRunResult& result) {
    if (request.requireWorkspaceChanges && !hasSuccessfulWorkspaceChange(result)) {
        return "当前任务要求完善项目，但尚未在安全副本中产生真实修改";
    }
    if (request.requireReaudit && !result.auditDiff.has_value()) {
        return "当前任务要求修改后复审，但尚未生成修改前后的确定性对比";
    }
    return {};
}

} // namespace

Result<AgentRunResult> BrainAgentLoop::run(const LlmConfig& config, const AgentRunRequest& request,
                                           std::size_t maxSteps) const {
    return run(config, request, {}, maxSteps);
}

Result<AgentRunResult> BrainAgentLoop::run(const LlmConfig& config, const AgentRunRequest& request,
                                           AgentEventObserver observe, std::size_t maxSteps) const {
    const auto configuredSteps = maxSteps == 0U ? config.maxAgentSteps : maxSteps;
    if (configuredSteps == 0U || configuredSteps > 256U) {
        return Result<AgentRunResult>::failure("智能体最大工具步数必须在 1 到 256 之间");
    }
    LlmBrain brain;
    return runWithDecisionProvider(
        request,
        [&](const AgentRunRequest& activeRequest, const AgentRunResult& result,
            const std::vector<AgentToolSpec>& tools) {
            return brain.decideNextAgentStep(config, activeRequest, result, tools);
        },
        configuredSteps, std::move(observe));
}

Result<AgentRunResult> BrainAgentLoop::runWithDecisionProvider(const AgentRunRequest& request,
                                                               AgentDecisionProvider decide,
                                                               std::size_t maxSteps,
                                                               AgentEventObserver observe) const {
    if (maxSteps == 0U || maxSteps > 256U) {
        return Result<AgentRunResult>::failure("智能体最大工具步数必须在 1 到 256 之间");
    }
    AgentRunResult result;
    result.plan.summary = "DeepSeek 原生工具循环：每一步基于已有观察选择一个受控工具，"
                          "或在信息足够时给出最终回答。";
    const auto stepLimit = maxSteps;
    AgentRuntime runtime;
    auto activeRequest = request;
    std::optional<AuditResult> ownedBaseline;
    std::string previousCallSignature;
    std::size_t consecutiveIdenticalCalls = 0U;

    for (std::size_t step = 1U; step <= stepLimit; ++step) {
        if (requestCancelled(activeRequest)) {
            return Result<AgentRunResult>::failure("智能体任务已取消");
        }
        const auto tools = availableTools(activeRequest);
        if (tools.empty()) {
            return Result<AgentRunResult>::failure("当前权限模式没有可供 DeepSeek 调用的工具");
        }
        constexpr std::size_t kMaximumDecisionAttempts = 3U;
        Result<AgentDecision> decision = Result<AgentDecision>::failure("尚未请求模型决策");
        for (std::size_t attempt = 1U; attempt <= kMaximumDecisionAttempts; ++attempt) {
            decision = decide(activeRequest, result, tools);
            if (decision.ok()) {
                break;
            }
            if (requestCancelled(activeRequest)) {
                return Result<AgentRunResult>::failure("智能体任务已取消");
            }
            if (!retryableDecisionFailure(decision.error())) {
                break;
            }
            if (attempt < kMaximumDecisionAttempts) {
                // Feed native response validation/transport failures back into the next request
                // without consuming an agent tool step.
                result.observations.push_back(AgentObservation{
                    .callId =
                        "decision_retry_" + std::to_string(step) + "_" + std::to_string(attempt),
                    .toolName = "model_decision_validation",
                    .ok = false,
                    .summary = "上一次 DeepSeek 响应不可用，请重新选择一个原生工具或给出最终文本",
                    .output = JsonValue::Object{{"attempt", static_cast<double>(attempt)},
                                                {"validation_error", decision.error()}}});
                result.trace = agentRunTraceJson(result);
            }
        }
        if (!decision.ok()) {
            return Result<AgentRunResult>::failure("智能助手在自动重试后仍无法生成有效决策: " +
                                                   decision.error());
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
            const auto incomplete = incompleteWorkflowReason(activeRequest, result);
            if (!incomplete.empty()) {
                AgentObservation required{
                    .callId = "workflow_required_" + std::to_string(step),
                    .toolName = activeRequest.requireReaudit ? "re_audit_repaired_project"
                                                             : "workspace_change",
                    .ok = false,
                    .summary = incomplete + "，不能提前结束",
                    .output = JsonValue::Object{
                        {"workspace_change_required", activeRequest.requireWorkspaceChanges},
                        {"reaudit_required", activeRequest.requireReaudit}}};
                result.observations.push_back(required);
                result.events.push_back(toolEvent(required));
                if (observe) {
                    observe(result.events.back());
                }
                result.trace = agentRunTraceJson(result);
                continue;
            }
            setAgentFinalAnswer(result, decision.value().finalAnswer, "DeepSeek final answer");
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
        const auto callSignature = call.name + "\n" + writeJson(call.input, 0);
        if (callSignature == previousCallSignature) {
            ++consecutiveIdenticalCalls;
        } else {
            previousCallSignature = callSignature;
            consecutiveIdenticalCalls = 1U;
        }
        if (consecutiveIdenticalCalls >= 3U) {
            return Result<AgentRunResult>::failure(
                "智能助手连续三次选择完全相同的工具和参数，已停止无效循环: " + call.name);
        }
        if (consecutiveIdenticalCalls == 2U) {
            // Keep the rejected native tool call in the in-memory transcript so DeepSeek thinking
            // mode receives its reasoning_content/tool_calls block together with this tool result.
            result.plan.calls.push_back(call);
            const auto tool = std::find_if(tools.begin(), tools.end(), [&](const auto& spec) {
                return spec.name == call.name;
            });
            AgentObservation repeated{
                .callId = call.id,
                .toolName = call.name,
                .ok = false,
                .summary = "相同工具和参数刚刚已经执行，请根据已有结果选择下一步",
                .output =
                    JsonValue::Object{{"reason", "duplicate_call"},
                                      {"input_schema", tool == tools.end() ? JsonValue::Object{}
                                                                           : tool->inputSchema}}};
            result.observations.push_back(repeated);
            result.events.push_back(toolEvent(repeated));
            if (observe) {
                observe(result.events.back());
            }
            result.trace = agentRunTraceJson(result);
            continue;
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

        const bool writesRepairedProject =
            call.name == "apply_repaired_text_edit" || call.name == "create_repaired_text_file";
        if (activeRequest.requireWorkspaceChanges && writesRepairedProject &&
            !hasSuccessfulProjectRead(result)) {
            AgentObservation readRequired{
                .callId = call.id,
                .toolName = call.name,
                .ok = false,
                .summary = "修改前必须先读取相关项目文件，不能根据文件名或摘要盲目改写",
                .output = JsonValue::Object{
                    {"required_tools",
                     JsonValue::Array{"read_text_file", "read_extracted_document"}}}};
            result.observations.push_back(readRequired);
            result.events.push_back(toolEvent(readRequired));
            if (observe) {
                observe(result.events.back());
            }
            result.trace = agentRunTraceJson(result);
            continue;
        }

        std::size_t streamedObservations = 0U;
        auto execution =
            runtime.runToolExecution(activeRequest, call, [&](const AgentObservation& observation) {
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
                ownedBaseline = *result.auditResult;
                activeRequest.baselineAuditResult = &(*ownedBaseline);
            }
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
            "DeepSeek 在最大步数内没有调用必需的 run_project_audit 工具");
    }
    const auto incomplete = incompleteWorkflowReason(activeRequest, result);
    if (!incomplete.empty()) {
        return Result<AgentRunResult>::failure(incomplete + "，已达到本轮最大工具步数");
    }
    return Result<AgentRunResult>::failure(
        "智能助手已达到本轮最大工具步数，但没有给出最终回答；已有工具结果仍然有效，可继续重试");
}

} // namespace cc
