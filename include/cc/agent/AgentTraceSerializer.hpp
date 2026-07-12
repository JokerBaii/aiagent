/**
 * @file AgentTraceSerializer.hpp
 * @brief Agent 调用、观察、事件和完整回合的稳定 JSON 序列化接口。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"

namespace cc {

[[nodiscard]] JsonValue agentToolCallJson(const AgentToolCall& call);
[[nodiscard]] JsonValue agentObservationJson(const AgentObservation& observation);

} // namespace cc
