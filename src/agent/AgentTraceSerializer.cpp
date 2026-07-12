/**
 * @file AgentTraceSerializer.cpp
 * @brief Agent trace 序列化与最终回答事件维护。
 */

#include "cc/agent/AgentTraceSerializer.hpp"

#include <utility>

namespace cc {

JsonValue agentToolCallJson(const AgentToolCall& call) {
    return JsonValue::Object{
        {"id", call.id}, {"name", call.name}, {"reason", call.reason}, {"input", call.input}};
}

JsonValue agentObservationJson(const AgentObservation& observation) {
    return JsonValue::Object{{"call_id", observation.callId},
                             {"tool_name", observation.toolName},
                             {"ok", observation.ok},
                             {"summary", observation.summary},
                             {"output", observation.output}};
}

std::string toString(AgentEventKind kind) {
    switch (kind) {
    case AgentEventKind::Plan:
        return "plan";
    case AgentEventKind::Tool:
        return "tool";
    case AgentEventKind::Assistant:
        return "assistant";
    case AgentEventKind::System:
        return "system";
    }
    return "system";
}

std::string toString(AgentDecisionKind kind) {
    switch (kind) {
    case AgentDecisionKind::ToolCall:
        return "tool_call";
    case AgentDecisionKind::FinalAnswer:
        return "final_answer";
    }
    return "tool_call";
}

JsonValue agentRunTraceJson(const AgentRunResult& result) {
    JsonValue::Array calls;
    for (const auto& call : result.plan.calls) {
        calls.push_back(agentToolCallJson(call));
    }
    JsonValue::Array observations;
    for (const auto& observation : result.observations) {
        observations.push_back(agentObservationJson(observation));
    }
    JsonValue::Array events;
    for (const auto& event : result.events) {
        events.push_back(JsonValue::Object{{"kind", toString(event.kind)},
                                           {"role", event.role},
                                           {"text", event.text},
                                           {"context", event.context},
                                           {"payload", event.payload}});
    }
    return JsonValue::Object{
        {"plan", JsonValue::Object{{"summary", result.plan.summary}, {"calls", JsonValue{calls}}}},
        {"observations", JsonValue{observations}},
        {"events", JsonValue{events}},
        {"final_answer", result.finalAnswer}};
}

void setAgentFinalAnswer(AgentRunResult& result, std::string finalAnswer, std::string context) {
    result.finalAnswer = std::move(finalAnswer);
    for (auto& event : result.events) {
        if (event.kind != AgentEventKind::Assistant) {
            continue;
        }
        event.text = result.finalAnswer;
        event.context = context;
        event.payload = JsonValue::Object{};
        result.trace = agentRunTraceJson(result);
        return;
    }
    result.events.push_back(AgentEvent{.kind = AgentEventKind::Assistant,
                                       .role = "智能体",
                                       .text = result.finalAnswer,
                                       .context = std::move(context),
                                       .payload = JsonValue::Object{}});
    result.trace = agentRunTraceJson(result);
}

} // namespace cc
