#include "../TestSupport.hpp"
#include "cc/llm/AdvisoryReconciler.hpp"
#include "cc/llm/LlmProviderProfile.hpp"

void runLlmProviderProfileTests() {
    cc::LlmProviderResolver resolver;

    auto openAi = resolver.resolve({{"OPENAI_API_KEY", "openai-secret"}});
    requireTrue(openAi.configured && openAi.config.provider == "openai",
                "an OpenAI key must select the complete OpenAI profile");
    requireTrue(openAi.config.endpoint == "https://api.openai.com/v1/chat/completions",
                "an OpenAI key must never retain the DeepSeek endpoint");
    requireTrue(openAi.config.apiKeyHeader == "Authorization" &&
                    openAi.config.apiKeyPrefix == "Bearer ",
                "OpenAI authentication should use a bearer header");

    auto anthropic = resolver.resolve({{"ANTHROPIC_API_KEY", "anthropic-secret"},
                                       {"OPENAI_API_KEY", "openai-secret"}});
    requireTrue(anthropic.config.provider == "anthropic",
                "the documented Anthropic credential should have deterministic priority");
    requireTrue(anthropic.config.endpoint == "https://api.anthropic.com/v1/messages",
                "an Anthropic key must select the Anthropic endpoint atomically");
    requireTrue(anthropic.config.apiKeyHeader == "x-api-key" &&
                    anthropic.config.apiKeyPrefix.empty(),
                "a standard Anthropic API key must use x-api-key without Bearer");

    auto workloadToken =
        resolver.resolve({{"ANTHROPIC_AUTH_TOKEN", "short-lived-token"}});
    requireTrue(workloadToken.config.apiKeyHeader == "Authorization" &&
                    workloadToken.config.apiKeyPrefix == "Bearer ",
                "an Anthropic workload token should use Authorization Bearer");

    auto deepSeek = resolver.resolve({{"DEEPSEEK_API_KEY", "deepseek-secret"},
                                      {"DEEPSEEK_BASE_URL", "https://proxy.example/v1/"},
                                      {"DEEPSEEK_MODEL", "custom-model"}});
    requireTrue(deepSeek.config.provider == "deepseek" && deepSeek.customEndpoint,
                "an explicit DeepSeek proxy should remain marked as custom");
    requireTrue(deepSeek.config.endpoint ==
                    "https://proxy.example/v1/chat/completions" &&
                    deepSeek.config.model == "custom-model",
                "provider overrides should be applied only to their own profile");

    auto empty = resolver.resolve({});
    requireTrue(!empty.configured && empty.config.apiKey.empty(),
                "credentials must not imply authorization when no key exists");

    cc::AuditResult deterministic;
    deterministic.trustScore.totalScore = 40;
    deterministic.findings.push_back({"RULE-1", cc::Severity::Blocker, "材料缺失", "缺材料",
                                      {}, {}, "补充材料"});
    cc::AuditAdvisory proposed;
    proposed.suggestedScore = 180;
    proposed.risks = {
        {.title = "材料缺失",
         .severity = cc::Severity::Blocker,
         .reason = "项目不通过",
         .ruleIdHint = "WRONG-RULE"},
        {.title = "材料缺失",
         .severity = cc::Severity::Info,
         .reason = "风险很低",
         .ruleIdHint = "RULE-1"},
        {.title = "局部结论",
         .severity = cc::Severity::Warning,
         .reason = "项目并非无风险，不能通过"}};
    const auto reconciled = cc::AdvisoryReconciler{}.reconcile(proposed, deterministic);
    requireTrue(reconciled.suggestedScore == 100,
                "defensive reconciliation must bound an invalid suggested score");
    requireTrue(reconciled.items[0].verdict == cc::AdvisoryVerdict::Unverified,
                "a wrong rule id must not be rescued by a generic matching title");
    requireTrue(reconciled.items[1].verdict == cc::AdvisoryVerdict::Conflicting,
                "a severity mismatch must not be marked confirmed");
    requireTrue(reconciled.items[2].verdict == cc::AdvisoryVerdict::Unverified,
                "negative wording containing the word pass must not become optimistic");
}
