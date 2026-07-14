/**
 * @file LlmBrain.hpp
 * @brief 可选大模型 Brain 编排。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/llm/LlmTypes.hpp"

namespace cc {

/**
 * @brief 可选大模型 Brain 编排器。
 *
 * LlmBrain 只能在 allowNetwork 和 allowLlm 同时为 true
 * 时调用外部接口；它可以生成受控工具决策，但最终评分仍由 RuleEngine 和
 * TrustScoreCalculator 裁决。
 */
class LlmBrain {
  public:
    /**
     * @brief 从当前 provider 的只读模型目录读取该凭证可用的模型 ID。
     *
     * model 字段可以为空；endpoint、认证方式和联网权限仍须有效。
     */
    [[nodiscard]] Result<std::vector<std::string>> listModels(const LlmConfig& config) const;
    /** @brief 解析 DeepSeek data[].id 模型目录结构。 */
    [[nodiscard]] Result<std::vector<std::string>>
    parseModelList(const std::string& responseBody) const;
    /**
     * @brief 纯本地构造并校验 provider 请求体，便于在联网前复核安全边界。
     */
    [[nodiscard]] Result<JsonValue> preparePayload(const LlmConfig& config,
                                                   const std::vector<LlmMessage>& messages) const;
    /** @brief 构造 DeepSeek 原生 tools/tool_calls 智能体请求体。 */
    [[nodiscard]] Result<JsonValue>
    prepareAgentPayload(const LlmConfig& config, const AgentRunRequest& request,
                        const AgentRunResult& result,
                        const std::vector<AgentToolSpec>& tools) const;
    /**
     * @brief 发送通用消息并返回模型响应。
     */
    [[nodiscard]] Result<LlmResponse> complete(const LlmConfig& config,
                                               const std::vector<LlmMessage>& messages) const;
    /**
     * @brief 根据已有工具观察结果生成下一步 Brain 决策。
     */
    [[nodiscard]] Result<AgentDecision>
    decideNextAgentStep(const LlmConfig& config, const AgentRunRequest& request,
                        const AgentRunResult& result,
                        const std::vector<AgentToolSpec>& tools) const;
    /** @brief 解析 DeepSeek Chat Completion 的原生 tool_calls 或最终文本。 */
    [[nodiscard]] Result<AgentDecision>
    parseAgentDecisionResponse(const std::string& responseBody) const;
};

} // namespace cc
