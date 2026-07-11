/**
 * @file LlmTypes.hpp
 * @brief 可选大模型 Brain 的配置和结果模型。
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief OpenAI-compatible 消息。
 */
struct LlmMessage {
    std::string role;
    std::string content;
};

/**
 * @brief LLM 调用配置。
 *
 * allowNetwork 和 allowLlm 是双重授权开关，默认 false。
 */
struct LlmConfig {
    std::string endpoint{"https://api.openai.com/v1/chat/completions"};
    std::string model{"gpt-4o-mini"};
    std::string apiKey;
    std::string provider{"openai"};
    std::string apiKeyHeader{"Authorization"};
    std::string apiKeyPrefix{"Bearer "};
    int maxTokens{4096};
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
