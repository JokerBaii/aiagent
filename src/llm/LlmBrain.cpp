/**
 * @file LlmBrain.cpp
 * @brief 可选大模型 Brain 编排实现。
 */

#include "cc/llm/LlmBrain.hpp"
#include "cc/core/Enums.hpp"
#include "cc/llm/EndpointParser.hpp"
#include "cc/llm/HttpsJsonClient.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cstddef>

namespace cc {
namespace {

[[nodiscard]] JsonValue messagesToJson(const std::vector<LlmMessage>& messages) {
    JsonValue::Array array;
    for (const auto& message : messages) {
        array.emplace_back(JsonValue::Object{{"role", message.role}, {"content", message.content}});
    }
    return JsonValue{array};
}

[[nodiscard]] std::string systemPromptFromMessages(const std::vector<LlmMessage>& messages) {
    std::string prompt;
    for (const auto& message : messages) {
        if (message.role != "system") {
            continue;
        }
        if (!prompt.empty()) {
            prompt += "\n\n";
        }
        prompt += message.content;
    }
    return prompt;
}

[[nodiscard]] JsonValue anthropicMessagesToJson(const std::vector<LlmMessage>& messages) {
    JsonValue::Array array;
    for (const auto& message : messages) {
        if (message.role == "system") {
            continue;
        }
        const auto role = message.role == "assistant" ? "assistant" : "user";
        array.emplace_back(JsonValue::Object{{"role", role}, {"content", message.content}});
    }
    return JsonValue{array};
}

[[nodiscard]] JsonValue compactAuditJson(const AuditResult* result) {
    if (result == nullptr) {
        return JsonValue::Object{{"available", false}};
    }
    const auto root = JsonReporter{}.toJson(*result);
    return JsonValue::Object{{"available", true},
                             {"summary", root.at("summary")},
                             {"cpir", root.at("cpir")},
                             {"findings", root.at("findings")},
                             {"evidence_matches", root.at("evidence_matches")},
                             {"fix_tasks", root.at("fix_tasks")}};
}

[[nodiscard]] JsonValue toolSpecsToJson(const std::vector<AgentToolSpec>& tools) {
    JsonValue::Array array;
    for (const auto& tool : tools) {
        array.push_back(JsonValue::Object{{"name", tool.name},
                                          {"description", tool.description},
                                          {"permission", toString(tool.permission)},
                                          {"input_schema", tool.inputSchema},
                                          {"output_schema", tool.outputSchema}});
    }
    return JsonValue{array};
}

[[nodiscard]] std::string agentStepSystemPrompt() {
    return "你是大学生项目材料审计平台的智能审计助手。你要"
           "在受控工具循环中逐步工作：先看已有 observations，再决定调用一个工具，"
           "或在信息足够时给出最终回答。只能输出一个 JSON object。调用工具时输出 "
           "{\"action\":\"tool\",\"summary\":string,\"call\":{\"id\":string,"
           "\"name\":string,\"reason\":string,\"input\":object}}；完成时输出 "
           "{\"action\":\"final\",\"summary\":string,\"final_answer\":string}。"
           "只能选择工具清单中的 name；不要要求自由执行 shell；不要读取项目外文件；"
           "不要覆盖原项目；需要产出代码、配置、报告或材料包内容时写入 workspace 工具；"
           "当 audit_required=true 且 audit_context.available=false 时，必须先调用 "
           "run_project_audit，让确定性规则引擎生成审计结果，禁止直接给出评审结论；"
           "run_project_audit 返回后要基于规则结果继续研判，必要时再读取或搜索项目文件；"
           "先利用文件列表里的 format/mime/language/text_readable 元数据判断能否读取；"
           "遇到 zip/tar/7z/tgz 等项目包时先 inspect_archive；遇到代码文件时可 read_text_file；"
           "如果当前 permission_mode 不允许某项能力，选择可用工具或在 final_answer "
           "中说明需要切换模式；"
           "不要伪造竞赛数据；不要改写最终评分。最终回答面向第一次准备大学生竞赛材料的"
           "学生：项目可能用于竞赛、大创、课程或毕业设计，也可能包含论文、专利、软著等"
           "成果材料。使用自然、具体的中文，先说明结论，再列最重要的 3 至 6 个问题和可执行"
           "下一步；解释专业词，不直接暴露 audit_context、run_project_audit、Brain step、"
           "RuleEngine 等内部字段或类名；把 blocker/warning、P0/P1 分别说成"
           "“必须处理/建议处理”和“最高优先级/较高优先级”。";
}

[[nodiscard]] std::string truncateText(const std::string& text, std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    return text.substr(0U, limit) + "\n...[已截断]";
}

[[nodiscard]] JsonValue compactObservationOutput(const AgentObservation& observation) {
    JsonValue::Object output;
    for (const auto& [key, value] : observation.output.asObject()) {
        if (key == "content" || key == "preview") {
            output.emplace(key, truncateText(value.asString(), 4000U));
        } else {
            output.emplace(key, value);
        }
    }
    return JsonValue{std::move(output)};
}

[[nodiscard]] JsonValue observationsToJson(const std::vector<AgentObservation>& observations) {
    JsonValue::Array array;
    for (const auto& observation : observations) {
        array.push_back(JsonValue::Object{{"tool_name", observation.toolName},
                                          {"ok", observation.ok},
                                          {"summary", observation.summary},
                                          {"output", compactObservationOutput(observation)}});
    }
    return JsonValue{array};
}

[[nodiscard]] JsonValue
conversationHistoryToJson(const std::vector<AgentConversationMessage>& history) {
    JsonValue::Array array;
    for (const auto& message : history) {
        array.push_back(
            JsonValue::Object{{"role", message.role}, {"content", message.content}});
    }
    return JsonValue{array};
}

[[nodiscard]] JsonValue planToJson(const AgentPlan& plan) {
    JsonValue::Array calls;
    for (const auto& call : plan.calls) {
        calls.push_back(JsonValue::Object{
            {"id", call.id}, {"name", call.name}, {"reason", call.reason}, {"input", call.input}});
    }
    return JsonValue::Object{{"summary", plan.summary}, {"calls", JsonValue{calls}}};
}

[[nodiscard]] std::vector<LlmMessage>
buildAgentNextStepMessages(const AgentRunRequest& request, const AgentRunResult& result,
                           const std::vector<AgentToolSpec>& tools) {
    const JsonValue capabilities =
        JsonValue::Object{{"read_project_files", true},
                          {"read_external_files", request.allowReadExternal},
                          {"write_workspace", true},
                          {"modify_original_project", request.allowModifyOriginal},
                          {"execute_command", request.allowExecuteCommand},
                          {"network_access", request.allowNetwork},
                          {"llm_access", request.allowLlm}};
    const JsonValue context =
        JsonValue::Object{{"user_goal", request.userGoal},
                          {"conversation_history",
                           conversationHistoryToJson(request.conversationHistory)},
                          {"project_root", util::pathString(request.projectRoot)},
                          {"workspace_root", util::pathString(request.workspaceRoot)},
                          {"permission_mode", request.permissionMode},
                          {"audit_required", request.requireAudit},
                          {"capabilities", capabilities},
                          {"audit_context", compactAuditJson(request.auditResult)},
                          {"tools", toolSpecsToJson(tools)},
                          {"current_plan", planToJson(result.plan)},
                          {"observations", observationsToJson(result.observations)}};
    const auto userPrompt =
        std::string{"请基于当前上下文输出下一步 decision。若还需要看文件或生成工作区产物，"
                    "只选择一个工具；若已经足够回答，输出 final。上下文：\n"} +
        writeJson(context, 2);
    return {LlmMessage{.role = "system", .content = agentStepSystemPrompt()},
            LlmMessage{.role = "user", .content = userPrompt}};
}

[[nodiscard]] std::string extractJsonObject(std::string content) {
    const auto fence = content.find("```");
    if (fence != std::string::npos) {
        const auto begin = content.find('{', fence);
        const auto endFence = content.rfind("```");
        if (begin != std::string::npos && endFence != std::string::npos && endFence > begin) {
            content = content.substr(begin, endFence - begin);
        }
    }
    const auto begin = content.find('{');
    const auto end = content.rfind('}');
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        return {};
    }
    return content.substr(begin, end - begin + 1U);
}

[[nodiscard]] Result<AgentToolCall> parseToolCall(const JsonValue& item, std::size_t index) {
    AgentToolCall call;
    call.id = item.at("id").asString();
    if (call.id.empty()) {
        call.id = "brain_" + std::to_string(index + 1U);
    }
    call.name = item.at("name").asString();
    call.reason = item.at("reason").asString();
    call.input = item.at("input").isObject() ? item.at("input") : JsonValue::Object{};
    if (call.name.empty()) {
        return Result<AgentToolCall>::failure("Brain 工具决策缺少 name");
    }
    return Result<AgentToolCall>::success(std::move(call));
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

[[nodiscard]] Result<LlmResponse> parseAnthropicResponse(const HttpResponse& response) {
    if (response.statusCode < 200 || response.statusCode >= 300) {
        return Result<LlmResponse>::failure("LLM HTTP 状态异常: " +
                                            std::to_string(response.statusCode));
    }
    auto parsed = parseJson(response.body);
    if (!parsed.ok()) {
        return Result<LlmResponse>::failure("LLM JSON 响应解析失败: " + parsed.error());
    }
    std::string content;
    const auto& blocks = parsed.value().at("content");
    if (blocks.isArray()) {
        for (const auto& block : blocks.asArray()) {
            const auto text = block.at("text").asString();
            if (text.empty()) {
                continue;
            }
            if (!content.empty()) {
                content += "\n";
            }
            content += text;
        }
    }
    if (content.empty()) {
        content = parsed.value().at("completion").asString();
    }
    if (content.empty()) {
        return Result<LlmResponse>::failure("LLM 响应缺少 content[].text");
    }
    return Result<LlmResponse>::success(LlmResponse{.content = content, .rawJson = response.body});
}

[[nodiscard]] std::string advisorySystemPrompt() {
    return "你是大学生竞赛项目可信审计的资深评委助手。系统已用确定性规则引擎完成审计，"
           "你要基于给定的审计上下文（资产、项目画像、规则风险、证据状态、补证任务）给出"
           "风险研判和评分建议。只能输出一个 JSON object，形如 "
           "{\"suggested_score\":number,\"overall_judgement\":string,\"risks\":[{\"title\":"
           "string,\"severity\":\"blocker|warning|info\",\"reason\":string,\"rule_id\":string,"
           "\"claim_id\":string,\"suggestion\":string}]}。rule_id/claim_id 填你认为对应的规则或"
           "声明编号，不确定就留空字符串。你的评分和研判只是建议，最终评分由确定性规则裁决；"
           "不要伪造用户、营收、合作、专利、实验或市场数据；不要凭空拔高，也不要无依据判为通过。";
}

[[nodiscard]] std::vector<LlmMessage> buildAdvisoryMessages(const AuditResult& result) {
    const auto root = JsonReporter{}.toJson(result);
    const JsonValue context = JsonValue::Object{
        {"summary", root.at("summary")},     {"cpir", root.at("cpir")},
        {"findings", root.at("findings")},   {"evidence_matches", root.at("evidence_matches")},
        {"fix_tasks", root.at("fix_tasks")}, {"deterministic_score", result.trustScore.totalScore}};
    const auto userPrompt =
        std::string{"这是确定性审计上下文，请给出你的风险研判和评分建议 JSON：\n"} +
        writeJson(context, 2);
    return {LlmMessage{.role = "system", .content = advisorySystemPrompt()},
            LlmMessage{.role = "user", .content = userPrompt}};
}

[[nodiscard]] AdvisoryRiskItem parseAdvisoryRisk(const JsonValue& item) {
    AdvisoryRiskItem risk;
    risk.title = item.at("title").asString();
    risk.severity = severityFromString(item.at("severity").asString());
    risk.reason = item.at("reason").asString();
    risk.ruleIdHint = item.at("rule_id").asString();
    risk.claimIdHint = item.at("claim_id").asString();
    risk.suggestion = item.at("suggestion").asString();
    return risk;
}

} // namespace

Result<LlmResponse> LlmBrain::complete(const LlmConfig& config,
                                       const std::vector<LlmMessage>& messages) const {
    // 即使 Workbench 默认允许联网和 LLM，这里仍要求运行时显式传入授权标志和
    // API key，避免无配置时发起外部请求。
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

    const auto provider = util::lowerAscii(config.provider);
    JsonValue::Object payloadObject;
    if (provider == "anthropic") {
        payloadObject = JsonValue::Object{{"model", config.model},
                                          {"max_tokens", config.maxTokens},
                                          {"system", systemPromptFromMessages(messages)},
                                          {"messages", anthropicMessagesToJson(messages)}};
    } else {
        payloadObject = JsonValue::Object{
            {"model", config.model}, {"messages", messagesToJson(messages)}, {"temperature", 0.2}};
    }
    const auto payload = writeJson(JsonValue{std::move(payloadObject)}, 0);
    std::vector<std::pair<std::string, std::string>> headers{
        {config.apiKeyHeader, config.apiKeyPrefix + config.apiKey}};
    if (provider == "anthropic") {
        headers.push_back({"anthropic-version", "2023-06-01"});
    }
    auto response = HttpsJsonClient{}.postJson(endpoint.value(), headers, payload);
    if (!response.ok()) {
        return Result<LlmResponse>::failure(response.error());
    }
    return provider == "anthropic" ? parseAnthropicResponse(response.value())
                                   : parseLlmResponse(response.value());
}

Result<AgentDecision> LlmBrain::decideNextAgentStep(const LlmConfig& config,
                                                    const AgentRunRequest& request,
                                                    const AgentRunResult& result,
                                                    const std::vector<AgentToolSpec>& tools) const {
    auto response = complete(config, buildAgentNextStepMessages(request, result, tools));
    if (!response.ok()) {
        return Result<AgentDecision>::failure(response.error());
    }
    return parseAgentDecision(response.value().content);
}

Result<AgentDecision> LlmBrain::parseAgentDecision(const std::string& content) const {
    const auto jsonText = extractJsonObject(content);
    if (jsonText.empty()) {
        return Result<AgentDecision>::failure("Brain 没有返回 JSON 决策");
    }
    auto parsed = parseJson(jsonText);
    if (!parsed.ok()) {
        return Result<AgentDecision>::failure(parsed.error());
    }

    const auto action = parsed.value().at("action").asString();
    AgentDecision decision;
    decision.summary = parsed.value().at("summary").asString();
    if (action == "final" || action == "answer") {
        decision.kind = AgentDecisionKind::FinalAnswer;
        decision.finalAnswer = parsed.value().at("final_answer").asString();
        if (decision.finalAnswer.empty()) {
            return Result<AgentDecision>::failure("Brain final 决策缺少 final_answer");
        }
        return Result<AgentDecision>::success(std::move(decision));
    }
    if (action == "tool" || action == "tool_call" || action == "call") {
        auto call = parseToolCall(parsed.value().at("call"), 0U);
        if (!call.ok()) {
            return Result<AgentDecision>::failure(call.error());
        }
        decision.kind = AgentDecisionKind::ToolCall;
        decision.call = std::move(call.value());
        if (decision.summary.empty()) {
            decision.summary = decision.call.reason;
        }
        return Result<AgentDecision>::success(std::move(decision));
    }
    return Result<AgentDecision>::failure("Brain 决策 action 必须是 tool 或 final");
}

Result<AuditAdvisory> LlmBrain::requestAuditAdvisory(const LlmConfig& config,
                                                     const AuditResult& result) const {
    auto response = complete(config, buildAdvisoryMessages(result));
    if (!response.ok()) {
        return Result<AuditAdvisory>::failure(response.error());
    }
    return parseAuditAdvisory(response.value().content);
}

Result<AuditAdvisory> LlmBrain::parseAuditAdvisory(const std::string& content) const {
    const auto jsonText = extractJsonObject(content);
    if (jsonText.empty()) {
        return Result<AuditAdvisory>::failure("LLM 没有返回研判 JSON");
    }
    auto parsed = parseJson(jsonText);
    if (!parsed.ok()) {
        return Result<AuditAdvisory>::failure(parsed.error());
    }

    AuditAdvisory advisory;
    advisory.suggestedScore = static_cast<int>(parsed.value().at("suggested_score").asNumber(0.0));
    advisory.overallJudgement = parsed.value().at("overall_judgement").asString();
    const auto& risks = parsed.value().at("risks");
    if (risks.isArray()) {
        for (const auto& item : risks.asArray()) {
            advisory.risks.push_back(parseAdvisoryRisk(item));
        }
    }
    return Result<AuditAdvisory>::success(std::move(advisory));
}

} // namespace cc
