/**
 * @file AuditPromptBuilder.hpp
 * @brief 审计结果的大模型提示构建。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/llm/LlmTypes.hpp"

namespace cc {

/**
 * @brief LLM Brain 的审计提示构建器。
 *
 * 提示只包含审计摘要、规则 ID、风险和补证任务，不把敏感文件正文传给外部模型。
 */
class AuditPromptBuilder {
  public:
    /**
     * @brief 从 C++ 审计结果构建 OpenAI-compatible 消息。
     */
    [[nodiscard]] std::vector<LlmMessage> buildFromResult(const AuditResult& result) const;
    /**
     * @brief 从 JSON 审计包构建 OpenAI-compatible 消息。
     */
    [[nodiscard]] std::vector<LlmMessage> buildFromAuditJson(const JsonValue& auditJson) const;
};

} // namespace cc
