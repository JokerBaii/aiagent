/**
 * @file LlmBrain.hpp
 * @brief 可选大模型 Brain 编排。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/llm/LlmTypes.hpp"

namespace cc {

/**
 * @brief 可选大模型 Brain 编排器。
 *
 * LlmBrain 只能在 allowNetwork 和 allowLlm 同时为 true 时调用外部接口；输出只作为建议，
 * 不参与 RuleEngine 或 TrustScoreCalculator 的最终裁决。
 */
class LlmBrain {
  public:
    /**
     * @brief 发送通用消息并返回模型响应。
     */
    [[nodiscard]] Result<LlmResponse> complete(const LlmConfig& config,
                                               const std::vector<LlmMessage>& messages) const;
    /**
     * @brief 基于 C++ 审计结果生成 Brain 建议。
     */
    [[nodiscard]] Result<LlmResponse> advise(const LlmConfig& config,
                                             const AuditResult& result) const;
    /**
     * @brief 基于 JSON 审计包生成 Brain 建议。
     */
    [[nodiscard]] Result<LlmResponse> advise(const LlmConfig& config,
                                             const JsonValue& auditJson) const;
};

} // namespace cc
