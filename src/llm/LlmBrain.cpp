/**
 * @file LlmBrain.cpp
 * @brief 可选大模型 Brain 编排实现。
 */

#include "cc/llm/LlmBrain.hpp"
#include "cc/llm/AuditPromptBuilder.hpp"
#include "cc/llm/EndpointParser.hpp"
#include "cc/llm/HttpsJsonClient.hpp"

namespace cc {
namespace {

[[nodiscard]] JsonValue messagesToJson(const std::vector<LlmMessage>& messages) {
    JsonValue::Array array;
    for (const auto& message : messages) {
        array.emplace_back(JsonValue::Object{{"role", message.role}, {"content", message.content}});
    }
    return JsonValue{array};
}

[[nodiscard]] Result<LlmResponse> parseLlmResponse(const HttpResponse& response) {
    if (response.statusCode < 200 || response.statusCode >= 300) {
        return Result<LlmResponse>::failure("LLM HTTP 状态异常: " +
                                            std::to_string(response.statusCode));
    }
    auto parsed = parseJson(response.body);
    if (!parsed.ok()) {
        return Result<LlmResponse>::failure("LLM JSON 响应解析失败: " + parsed.error());
    }
    const auto content = parsed.value().at("choices").at(0).at("message").at("content").asString();
    if (content.empty()) {
        return Result<LlmResponse>::failure("LLM 响应缺少 choices[0].message.content");
    }
    return Result<LlmResponse>::success(LlmResponse{.content = content, .rawJson = response.body});
}

} // namespace

Result<LlmResponse> LlmBrain::complete(const LlmConfig& config,
                                       const std::vector<LlmMessage>& messages) const {
    // 默认拒绝联网和 LLM；只有 Workbench 明确传入授权标志时才会调用外部 endpoint。
    if (!config.allowNetwork || !config.allowLlm) {
        return Result<LlmResponse>::failure("未授权联网或 LLM 调用，已阻止大模型 Brain");
    }
    if (config.apiKey.empty()) {
        return Result<LlmResponse>::failure("缺少 LLM API key");
    }
    auto endpoint = EndpointParser{}.parse(config.endpoint);
    if (!endpoint.ok()) {
        return Result<LlmResponse>::failure(endpoint.error());
    }

    const auto payload = writeJson(JsonValue::Object{{"model", config.model},
                                                     {"messages", messagesToJson(messages)},
                                                     {"temperature", 0.2}},
                                   0);
    const std::vector<std::pair<std::string, std::string>> headers{
        {"Authorization", "Bearer " + config.apiKey}};
    auto response = HttpsJsonClient{}.postJson(endpoint.value(), headers, payload);
    if (!response.ok()) {
        return Result<LlmResponse>::failure(response.error());
    }
    return parseLlmResponse(response.value());
}

Result<LlmResponse> LlmBrain::advise(const LlmConfig& config, const AuditResult& result) const {
    return complete(config, AuditPromptBuilder{}.buildFromResult(result));
}

Result<LlmResponse> LlmBrain::advise(const LlmConfig& config, const JsonValue& auditJson) const {
    return complete(config, AuditPromptBuilder{}.buildFromAuditJson(auditJson));
}

} // namespace cc
