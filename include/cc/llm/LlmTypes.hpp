/**
 * @file LlmTypes.hpp
 * @brief 可选大模型 Brain 的配置和结果模型。
 */

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cc {

/** @brief DeepSeek Chat Completion 普通文本消息。 */
struct LlmMessage {
    std::string role;
    std::string content;
};

/**
 * @brief DeepSeek 调用配置。
 *
 * allowNetwork 和 allowLlm 是每次任务内部的能力快照，默认 false；Workbench 在完整配置
 * 通过校验后自动为模型任务设置它们，不对应额外的用户确认开关。
 */
struct LlmConfig {
    std::string endpoint;
    std::string model;
    std::string apiKey;
    std::string provider;
    std::string apiKeyHeader;
    std::string apiKeyPrefix;
    int maxTokens{4096};
    /** Serialized prompt guard; provider token windows remain authoritative. */
    std::size_t maxPromptBytes{std::size_t{4U} * 1024U * 1024U};
    /** Maximum model decisions in one controlled tool turn. */
    std::size_t maxAgentSteps{32U};
    /** Omitted by default because some reasoning/custom models reject temperature. */
    std::optional<double> temperature;
    bool allowNetwork{false};
    bool allowLlm{false};
    std::function<bool()> isCancelled;
};

/**
 * @brief LLM 响应内容和原始 JSON。
 */
struct LlmResponse {
    std::string content;
    std::string rawJson;
};

} // namespace cc
