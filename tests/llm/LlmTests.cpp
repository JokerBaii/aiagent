/**
 * @file LlmTests.cpp
 * @brief llm 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/llm/AdvisoryReconciler.hpp"
#include "cc/llm/BrainAgentLoop.hpp"
#include "cc/llm/EndpointParser.hpp"
#include "cc/llm/HttpResponseParser.hpp"
#include "cc/llm/HttpsJsonClient.hpp"
#include "cc/llm/LlmBrain.hpp"
#include "cc/llm/LlmPromptGuard.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <chrono>

void runLlmTests() {
    auto endpoint = cc::EndpointParser{}.parse("https://api.example.com:8443/v1/chat");
    requireTrue(endpoint.ok(), "https endpoint should parse");
    requireTrue(endpoint.value().host == "api.example.com", "endpoint host mismatch");
    requireTrue(endpoint.value().port == "8443", "endpoint port mismatch");
    requireTrue(endpoint.value().target == "/v1/chat", "endpoint target mismatch");
    requireTrue(endpoint.value().hostHeader == "api.example.com:8443",
                "non-default port must be retained in Host header");
    auto defaultPortEndpoint =
        cc::EndpointParser{}.parse("https://api.example.com:443/v1/chat?stream=false");
    requireTrue(defaultPortEndpoint.ok() &&
                    defaultPortEndpoint.value().hostHeader == "api.example.com",
                "default HTTPS port should be normalized out of Host header");
    auto ipv6Endpoint = cc::EndpointParser{}.parse("https://[2001:db8::1]:8443/v1/chat");
    requireTrue(ipv6Endpoint.ok() && ipv6Endpoint.value().host == "2001:db8::1" &&
                    ipv6Endpoint.value().hostHeader == "[2001:db8::1]:8443",
                "bracketed IPv6 endpoint should parse canonically");
    for (const auto& invalidEndpoint : {
             "http://api.example.com/v1/chat",
             "https://user:pass@api.example.com/v1/chat",
             "https://api.example.com/v1/chat#secret",
             "https://api.example.com:0/v1/chat",
             "https://api.example.com:65536/v1/chat",
             "https://api.example.com:not-a-port/v1/chat",
             "https://api.example.com/v1/ chat",
             "https://api.example.com/v1/chat\r\nX-Evil: yes",
             "https://2001:db8::1/v1/chat",
             "https://[not-ipv6]/v1/chat",
         }) {
        requireTrue(!cc::EndpointParser{}.parse(invalidEndpoint).ok(),
                    std::string{"unsafe endpoint should be rejected: "} + invalidEndpoint);
    }

    cc::HttpsRequestOptions cancelledOptions;
    cancelledOptions.isCancelled = [] { return true; };
    auto cancelledRequest = cc::HttpsJsonClient{}.postJson(
        endpoint.value(), {{"Authorization", "Bearer offline-test-key"}}, "{}", cancelledOptions);
    requireTrue(!cancelledRequest.ok() &&
                    cancelledRequest.error().find("取消") != std::string::npos,
                "pre-cancelled HTTPS request must fail before network access");
    cancelledOptions.isCancelled = []() -> bool { throw std::runtime_error{"cancel callback"}; };
    requireTrue(!cc::HttpsJsonClient{}
                     .postJson(endpoint.value(), {{"Authorization", "Bearer offline-test-key"}},
                               "{}", cancelledOptions)
                     .ok(),
                "throwing cancellation callback must fail closed");
    auto injectedHeader = cc::HttpsJsonClient{}.postJson(
        endpoint.value(), {{"Authorization", "Bearer safe\r\nX-Evil: yes"}}, "{}");
    requireTrue(!injectedHeader.ok(), "CRLF header injection must fail before network access");
    auto reservedHeader =
        cc::HttpsJsonClient{}.postJson(endpoint.value(), {{"Host", "evil.example"}}, "{}");
    requireTrue(!reservedHeader.ok(), "caller must not override protocol-owned headers");
    cc::HttpsRequestOptions tinyRequestLimit;
    tinyRequestLimit.maxRequestBodyBytes = 4U;
    auto oversizedRequest = cc::HttpsJsonClient{}.postJson(
        endpoint.value(), {{"Authorization", "Bearer offline-test-key"}}, "12345",
        tinyRequestLimit);
    requireTrue(!oversizedRequest.ok(), "oversized HTTPS body must fail before network access");
    cc::HttpsRequestOptions invalidTimeout;
    invalidTimeout.totalTimeout = std::chrono::milliseconds{0};
    auto invalidTimeoutRequest = cc::HttpsJsonClient{}.postJson(
        endpoint.value(), {{"Authorization", "Bearer offline-test-key"}}, "{}", invalidTimeout);
    requireTrue(!invalidTimeoutRequest.ok(), "non-positive total timeout must be rejected");
    cc::HttpsRequestOptions excessiveResponseLimit;
    excessiveResponseLimit.maxResponseBytes = 65U * 1024U * 1024U;
    requireTrue(!cc::HttpsJsonClient{}
                     .postJson(endpoint.value(), {{"Authorization", "Bearer offline-test-key"}},
                               "{}", excessiveResponseLimit)
                     .ok(),
                "response limit itself must have a hard upper bound");

    const std::string rawResponse = "HTTP/1.1 200 OK\r\nTransfer-Encoding: "
                                    "chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n";
    requireTrue(cc::HttpResponseParser{}.isComplete(rawResponse),
                "complete chunked response should be detected before socket close");
    requireTrue(!cc::HttpResponseParser{}.isComplete(
                    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhel"),
                "partial chunked response should stay incomplete");
    auto parsed = cc::HttpResponseParser{}.parse(rawResponse);
    requireTrue(parsed.ok() && parsed.value().body == "hello", "chunked response should parse");
    const std::string trailerResponse = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                                        "5;ext=value\r\nhello\r\n0\r\nX-Trace: done\r\n\r\n";
    requireTrue(cc::HttpResponseParser{}.isComplete(trailerResponse) &&
                    cc::HttpResponseParser{}.parse(trailerResponse).ok(),
                "valid chunk extensions and trailers should parse");
    requireTrue(!cc::HttpResponseParser{}
                     .parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                            "5\r\nhelloX\r\n0\r\n\r\n")
                     .ok(),
                "chunk framing mismatch must be rejected");

    const std::string contentLengthResponse =
        "HTTP/1.1 401 Unauthorized\r\nContent-Length: 5\r\n\r\nerror";
    requireTrue(cc::HttpResponseParser{}.isComplete(contentLengthResponse),
                "content-length response should be detected before socket close");
    requireTrue(!cc::HttpResponseParser{}.isComplete(
                    "HTTP/1.1 401 Unauthorized\r\nContent-Length: 5\r\n\r\nerr"),
                "partial content-length response should stay incomplete");
    requireTrue(
        !cc::HttpResponseParser{}.parse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nerr").ok(),
        "truncated content-length response must not parse as success");
    requireTrue(!cc::HttpResponseParser{}
                     .parse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n"
                            "Content-Length: 2\r\n\r\na")
                     .ok(),
                "duplicate response lengths must be rejected");
    requireTrue(!cc::HttpResponseParser{}
                     .parse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n"
                            "Transfer-Encoding: chunked\r\n\r\na")
                     .ok(),
                "conflicting transfer framing must be rejected");

    cc::AuditResult result;
    result.cpir.projectName = "Demo";
    cc::AuditFinding finding;
    finding.ruleId = "RULE-001";
    finding.reason = "缺少证据";
    result.findings = {finding};
    const std::vector<cc::LlmMessage> messages{{.role = "system", .content = "只输出 JSON。"},
                                               {.role = "user", .content = "检查 RULE-001。"}};

    cc::LlmConfig config;
    config.apiKey = "test-key";
    auto blocked = cc::LlmBrain{}.complete(config, messages);
    requireTrue(!blocked.ok(), "llm brain should block without explicit permission");
    cc::LlmConfig unsafeHeaderConfig = config;
    unsafeHeaderConfig.allowNetwork = true;
    unsafeHeaderConfig.allowLlm = true;
    unsafeHeaderConfig.apiKeyHeader = "Authorization\r\nX-Evil";
    requireTrue(!cc::LlmBrain{}.complete(unsafeHeaderConfig, messages).ok(),
                "LLM config header injection must fail before network access");
    cc::LlmConfig throwingCancellationConfig = config;
    throwingCancellationConfig.allowNetwork = true;
    throwingCancellationConfig.allowLlm = true;
    throwingCancellationConfig.isCancelled = []() -> bool {
        throw std::runtime_error{"cancel callback"};
    };
    requireTrue(!cc::LlmBrain{}.complete(throwingCancellationConfig, messages).ok(),
                "LLM cancellation callback must fail closed");

    const std::string privateKey =
        "-----BEGIN PRIVATE KEY-----\nnot-a-real-private-key\n-----END PRIVATE KEY-----";
    const auto redacted = cc::LlmPromptGuard{}.redactSecrets(
        "api_key=plain-secret, password=another-secret\n"
        "Authorization: Bearer bearer-secret\n"
        "free form Bearer abcdefghijklmnop\n"
        "jwt eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.signature123\n"
        "token sk-example123456789 ghp_1234567890abcdef\n"
        "database=postgres://admin:db-password@example.invalid/db\n" +
        privateKey);
    for (const auto& leaked :
         {"plain-secret", "another-secret", "bearer-secret", "abcdefghijklmnop",
          "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.signature123", "sk-example123456789",
          "ghp_1234567890abcdef", "db-password", "not-a-real-private-key"}) {
        requireTrue(redacted.find(leaked) == std::string::npos,
                    std::string{"prompt guard leaked secret: "} + leaked);
    }

    std::vector<cc::LlmMessage> oversizedHistory;
    oversizedHistory.push_back({.role = "system", .content = "persistent-system-policy"});
    for (int index = 0; index < 60; ++index) {
        oversizedHistory.push_back(
            {.role = index % 2 == 0 ? "user" : "assistant",
             .content = "turn-" + std::to_string(index) + " " + std::string(20000U, 'x')});
    }
    const auto boundedHistory = cc::LlmPromptGuard{}.sanitize(oversizedHistory);
    std::size_t boundedHistoryBytes = 0U;
    std::string boundedHistoryText;
    for (const auto& message : boundedHistory) {
        boundedHistoryBytes += message.content.size();
        boundedHistoryText += message.content;
    }
    requireTrue(boundedHistory.size() <= 32U && boundedHistoryBytes <= 96U * 1024U,
                "prompt history must stay within message and byte budgets");
    requireTrue(boundedHistoryText.find("persistent-system-policy") != std::string::npos &&
                    boundedHistoryText.find("turn-59") != std::string::npos,
                "prompt budget must retain the system policy and newest turn");
    requireTrue(boundedHistoryText.find("turn-0 ") == std::string::npos,
                "prompt budget should discard stale turns");

    cc::LlmConfig openAiPayloadConfig;
    openAiPayloadConfig.provider = "openai";
    openAiPayloadConfig.model = "offline-model";
    openAiPayloadConfig.maxTokens = 3210;
    openAiPayloadConfig.apiKey = "custom-secret-value";
    auto openAiPayload = cc::LlmBrain{}.preparePayload(
        openAiPayloadConfig, {{.role = "user", .content = "never forward custom-secret-value"}});
    requireTrue(openAiPayload.ok() && openAiPayload.value().at("max_tokens").asNumber() == 3210.0,
                "OpenAI-compatible payload must include max_tokens");
    requireTrue(cc::writeJson(openAiPayload.value(), 0).find("custom-secret-value") ==
                    std::string::npos,
                "configured API key must be removed from model messages");
    cc::LlmConfig deepSeekPayloadConfig = openAiPayloadConfig;
    deepSeekPayloadConfig.provider = "deepseek";
    auto deepSeekPayload = cc::LlmBrain{}.preparePayload(deepSeekPayloadConfig, messages);
    requireTrue(deepSeekPayload.ok() &&
                    deepSeekPayload.value().at("max_tokens").asNumber() == 3210.0,
                "every OpenAI-compatible provider must receive max_tokens");
    cc::LlmConfig anthropicPayloadConfig = openAiPayloadConfig;
    anthropicPayloadConfig.provider = "anthropic";
    auto anthropicPayload = cc::LlmBrain{}.preparePayload(anthropicPayloadConfig, messages);
    requireTrue(anthropicPayload.ok() &&
                    anthropicPayload.value().at("max_tokens").asNumber() == 3210.0 &&
                    anthropicPayload.value().at("system").isString(),
                "Anthropic payload must separate system text and include max_tokens");
    openAiPayloadConfig.maxTokens = 0;
    requireTrue(!cc::LlmBrain{}.preparePayload(openAiPayloadConfig, messages).ok(),
                "zero max_tokens must be rejected locally");
    openAiPayloadConfig.maxTokens = 65537;
    requireTrue(!cc::LlmBrain{}.preparePayload(openAiPayloadConfig, messages).ok(),
                "excessive max_tokens must be rejected locally");

    const std::string toolDecisionJson =
        R"({"action":"tool","summary":"先读取 README","call":{"id":"s1","name":"read_text_file","reason":"需要看到文档正文","input":{"path":"README.md","max_bytes":4000}}})";
    auto toolDecision = cc::LlmBrain{}.parseAgentDecision(toolDecisionJson);
    requireTrue(toolDecision.ok(), "llm brain should parse tool decision json");
    requireTrue(toolDecision.value().kind == cc::AgentDecisionKind::ToolCall,
                "tool decision should request a tool call");
    requireTrue(toolDecision.value().call.name == "read_text_file",
                "tool decision should keep selected tool name");
    requireTrue(
        !cc::LlmBrain{}
             .parseAgentDecision(
                 R"({"action":"tool","summary":"bad","call":{"name":"read_text_file","reason":"missing input"}})")
             .ok(),
        "tool decision without an input object must be rejected");
    requireTrue(
        !cc::LlmBrain{}
             .parseAgentDecision(
                 R"({"action":"tool","summary":"bad","call":{"name":"bad name\r\n","reason":"invalid","input":{}}})")
             .ok(),
        "tool decision with an unsafe tool name must be rejected");

    const std::string finalDecisionJson =
        R"({"action":"final","summary":"已经足够回答","final_answer":"已完成受控检查。"})";
    auto finalDecision = cc::LlmBrain{}.parseAgentDecision(finalDecisionJson);
    requireTrue(finalDecision.ok(), "llm brain should parse final decision json");
    requireTrue(finalDecision.value().kind == cc::AgentDecisionKind::FinalAnswer,
                "final decision should stop the loop");
    requireTrue(finalDecision.value().finalAnswer.find("已完成") != std::string::npos,
                "final decision should keep final answer text");

    cc::AgentRunRequest request;
    request.userGoal = "检查文档";
    cc::AgentRunResult run;
    auto stepBlocked = cc::LlmBrain{}.decideNextAgentStep(config, request, run, {});
    requireTrue(!stepBlocked.ok(), "llm brain step should block without explicit permission");
    auto loopBlocked = cc::BrainAgentLoop{}.run(config, request);
    requireTrue(!loopBlocked.ok(), "brain agent loop should block without explicit permission");

    const auto loopRoot = std::filesystem::temp_directory_path() / "contest_brain_audit_loop_test";
    std::filesystem::remove_all(loopRoot);
    std::filesystem::create_directories(loopRoot);
    requireTrue(cc::util::writeTextFile(loopRoot / "README.md",
                                        "# 项目说明\n本项目参加大学生创新创业竞赛。\n")
                    .ok(),
                "brain loop audit fixture should be written");
    cc::AgentRunRequest auditLoopRequest;
    auditLoopRequest.userGoal = "审查当前项目";
    auditLoopRequest.projectRoot = loopRoot;
    auditLoopRequest.auditOptions.rulesDir = sourceDir() / "rules";
    auditLoopRequest.requireAudit = true;
    auditLoopRequest.conversationHistory = {
        {.role = "user", .content = "先审查项目"},
        {.role = "assistant", .content = "我会调用规则审计工具"}};
    int decisionCount = 0;
    std::vector<cc::AgentEvent> streamedEvents;
    auto integratedLoop = cc::BrainAgentLoop{}.runWithDecisionProvider(
        auditLoopRequest,
        [&](const cc::AgentRunRequest& activeRequest, const cc::AgentRunResult& current,
            const std::vector<cc::AgentToolSpec>& tools) {
            ++decisionCount;
            if (decisionCount == 1) {
                requireTrue(activeRequest.requireAudit,
                            "first brain step should know that audit is required");
                requireTrue(activeRequest.conversationHistory.size() == 2U,
                            "brain step should receive prior conversation turns");
                const auto auditTool =
                    std::find_if(tools.begin(), tools.end(), [](const cc::AgentToolSpec& tool) {
                        return tool.name == "run_project_audit";
                    });
                requireTrue(auditTool != tools.end(),
                            "brain tool list should expose deterministic project audit");
                return cc::Result<cc::AgentDecision>::success(
                    cc::AgentDecision{.kind = cc::AgentDecisionKind::ToolCall,
                                      .summary = "先运行规则审计",
                                      .call = cc::AgentToolCall{.id = "audit_step",
                                                                .name = "run_project_audit",
                                                                .reason = "取得规则与证据结果",
                                                                .input = cc::JsonValue::Object{}},
                                      .finalAnswer = {}});
            }
            requireTrue(activeRequest.auditResult != nullptr,
                        "audit result should be available to the next brain step");
            requireTrue(!activeRequest.requireAudit,
                        "audit requirement should clear after the tool succeeds");
            requireTrue(!current.observations.empty(),
                        "brain should receive deterministic stage observations");
            requireTrue(streamedEvents.size() >= cc::StagedAuditPipeline::stages().size() + 2U,
                        "audit stages should stream before the next model decision");
            return cc::Result<cc::AgentDecision>::success(
                cc::AgentDecision{.kind = cc::AgentDecisionKind::FinalAnswer,
                                  .summary = "基于规则结果回答",
                                  .call = {},
                                  .finalAnswer = "已根据规则和证据完成审查。"});
        },
        12U, [&](const cc::AgentEvent& event) { streamedEvents.push_back(event); });
    requireTrue(integratedLoop.ok(), "brain audit tool loop should complete");
    requireTrue(integratedLoop.value().auditResult.has_value(),
                "brain loop should return the typed audit result to the controller");
    requireTrue(decisionCount == 2,
                "brain should continue once after receiving deterministic audit observations");
    requireTrue(streamedEvents.size() == integratedLoop.value().events.size(),
                "each brain event should stream exactly once");
    requireTrue(streamedEvents.size() > cc::StagedAuditPipeline::stages().size(),
                "stream should include plan, every audit stage, summary and final answer");

    cc::AgentRunRequest optimizeRequest = auditLoopRequest;
    optimizeRequest.userGoal = "完善项目说明并重新检查";
    optimizeRequest.allowWriteWorkspace = true;
    optimizeRequest.requireWorkspaceChanges = true;
    optimizeRequest.requireReaudit = true;
    int optimizeStep = 0;
    auto optimizedLoop = cc::BrainAgentLoop{}.runWithDecisionProvider(
        optimizeRequest,
        [&](const cc::AgentRunRequest&, const cc::AgentRunResult&,
            const std::vector<cc::AgentToolSpec>&) {
            ++optimizeStep;
            cc::AgentDecision decision;
            decision.kind = cc::AgentDecisionKind::ToolCall;
            decision.summary = "继续完成受控优化闭环";
            decision.call.id = "optimize_" + std::to_string(optimizeStep);
            decision.call.reason = "测试真实修改与复审";
            decision.call.input = cc::JsonValue::Object{};
            if (optimizeStep == 1) {
                decision.call.name = "run_project_audit";
            } else if (optimizeStep == 2) {
                decision.call.name = "prepare_repaired_workspace";
            } else if (optimizeStep == 3) {
                decision.kind = cc::AgentDecisionKind::FinalAnswer;
                decision.finalAnswer = "尚未修改但试图提前结束";
            } else if (optimizeStep == 4) {
                decision.call.name = "apply_repaired_text_edit";
                decision.call.input =
                    cc::JsonValue::Object{{"path", "README.md"},
                                          {"expected_text", "# 项目说明\n"},
                                          {"replacement_text", "# 项目说明\n\n## 盲目修改\n"},
                                          {"expected_occurrences", 1}};
            } else if (optimizeStep == 5) {
                decision.call.name = "read_text_file";
                decision.call.input =
                    cc::JsonValue::Object{{"path", "README.md"}, {"max_bytes", 4000}};
            } else if (optimizeStep == 6) {
                decision.call.name = "apply_repaired_text_edit";
                decision.call.input =
                    cc::JsonValue::Object{{"path", "README.md"},
                                          {"expected_text", "# 项目说明\n"},
                                          {"replacement_text", "# 项目说明\n\n## 参赛材料说明\n"},
                                          {"expected_occurrences", 1}};
            } else if (optimizeStep == 7) {
                decision.kind = cc::AgentDecisionKind::FinalAnswer;
                decision.finalAnswer = "已修改但试图跳过复审";
            } else if (optimizeStep == 8) {
                decision.call.name = "re_audit_repaired_project";
            } else {
                decision.kind = cc::AgentDecisionKind::FinalAnswer;
                decision.finalAnswer = "已完成真实修改和修改后复审。";
            }
            return cc::Result<cc::AgentDecision>::success(std::move(decision));
        },
        10U);
    requireTrue(optimizedLoop.ok(), "optimization loop should complete the enforced workflow");
    requireTrue(optimizedLoop.value().auditDiff.has_value(),
                "optimization loop should return a typed before/after audit diff");
    requireTrue(optimizeStep == 9,
                "early final answers must be rejected until both edit and re-audit complete");

    int repeatedSteps = 0;
    auto repeatedLoop = cc::BrainAgentLoop{}.runWithDecisionProvider(
        auditLoopRequest,
        [&](const cc::AgentRunRequest&, const cc::AgentRunResult&,
            const std::vector<cc::AgentToolSpec>&) {
            ++repeatedSteps;
            return cc::Result<cc::AgentDecision>::success(cc::AgentDecision{
                .kind = cc::AgentDecisionKind::ToolCall,
                .summary = "重复调用",
                .call = cc::AgentToolCall{.id = "repeat_" + std::to_string(repeatedSteps),
                                          .name = "run_project_audit",
                                          .reason = "重复",
                                          .input = cc::JsonValue::Object{}},
                .finalAnswer = {}});
        },
        6U);
    requireTrue(!repeatedLoop.ok() && repeatedSteps == 3,
                "three identical decisions should stop instead of burning the whole step budget");

    // 混合研判：解析 LLM 研判 JSON，再用确定性结果校验。
    const std::string advisoryJson =
        R"({"suggested_score":90,"overall_judgement":"整体不错","risks":[)"
        R"({"title":"缺少证据","severity":"blocker","reason":"营收无支撑","rule_id":"RULE-001","claim_id":"","suggestion":"补充流水"},)"
        R"({"title":"项目整体通过","severity":"info","reason":"项目整体没有问题，可以通过提交","rule_id":"","claim_id":"","suggestion":""})"
        R"(]})";
    auto advisory = cc::LlmBrain{}.parseAuditAdvisory(advisoryJson);
    requireTrue(advisory.ok(), "advisory json should parse");
    requireTrue(advisory.value().risks.size() == 2U, "advisory should keep both risks");
    requireTrue(advisory.value().suggestedScore == 90, "advisory should keep suggested score");
    for (
        const auto& invalidAdvisory : {
            R"({"overall_judgement":"missing score","risks":[]})",
            R"({"suggested_score":"90","overall_judgement":"wrong type","risks":[]})",
            R"({"suggested_score":90.5,"overall_judgement":"fraction","risks":[]})",
            R"({"suggested_score":101,"overall_judgement":"range","risks":[]})",
            R"({"suggested_score":90,"overall_judgement":"missing risks"})",
            R"({"suggested_score":90,"overall_judgement":"bad item","risks":["risk"]})",
            R"({"suggested_score":90,"overall_judgement":"bad severity","risks":[{"title":"risk","severity":"critical","reason":"reason","rule_id":"","claim_id":"","suggestion":""}]})",
            R"({"suggested_score":90,"overall_judgement":"missing field","risks":[{"title":"risk","severity":"warning","reason":"reason","rule_id":"","claim_id":""}]})",
        }) {
        requireTrue(!cc::LlmBrain{}.parseAuditAdvisory(invalidAdvisory).ok(),
                    std::string{"malformed advisory should be rejected: "} + invalidAdvisory);
    }
    cc::JsonValue::Array tooManyRisks;
    for (std::size_t index = 0U; index < 101U; ++index) {
        tooManyRisks.push_back(cc::JsonValue::Object{{"title", "risk"},
                                                     {"severity", "warning"},
                                                     {"reason", "reason"},
                                                     {"rule_id", ""},
                                                     {"claim_id", ""},
                                                     {"suggestion", ""}});
    }
    const auto tooManyAdvisory =
        cc::writeJson(cc::JsonValue::Object{{"suggested_score", 90},
                                            {"overall_judgement", "bounded risks"},
                                            {"risks", cc::JsonValue{std::move(tooManyRisks)}}},
                      0);
    requireTrue(!cc::LlmBrain{}.parseAuditAdvisory(tooManyAdvisory).ok(),
                "advisory risk count must be bounded");
    const auto oversizedJudgement =
        cc::writeJson(cc::JsonValue::Object{{"suggested_score", 90},
                                            {"overall_judgement", std::string(8001U, 'x')},
                                            {"risks", cc::JsonValue::Array{}}},
                      0);
    requireTrue(!cc::LlmBrain{}.parseAuditAdvisory(oversizedJudgement).ok(),
                "oversized advisory judgement must be rejected");

    cc::AuditResult scored;
    scored.trustScore.totalScore = 55;
    cc::AuditFinding blockerFinding;
    blockerFinding.ruleId = "RULE-001";
    blockerFinding.severity = cc::Severity::Blocker;
    blockerFinding.title = "缺少证据";
    blockerFinding.reason = "营收声明缺少支撑材料";
    scored.findings = {blockerFinding};

    auto reconciled = cc::AdvisoryReconciler{}.reconcile(advisory.value(), scored);
    requireTrue(reconciled.finalScore == 55, "final score must come from deterministic result");
    requireTrue(reconciled.suggestedScore == 90,
                "reconciled should keep llm suggestion for compare");
    requireTrue(reconciled.confirmedCount == 1U, "risk matching a rule id should be confirmed");
    requireTrue(reconciled.conflictingCount == 1U,
                "optimistic 'pass' risk should conflict with a blocker and be downgraded");
    requireTrue(reconciled.items.size() == 2U, "all advisory items should be reconciled");
}
