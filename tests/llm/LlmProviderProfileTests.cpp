#include "../TestSupport.hpp"
#include "cc/llm/AdvisoryReconciler.hpp"
#include "cc/llm/LlmProviderProfile.hpp"

void runLlmProviderProfileTests() {
    cc::LlmProviderResolver resolver;

    auto openAi = resolver.resolve(
        {{"OPENAI_API_KEY", "openai-secret"}, {"OPENAI_MODEL", "account-model-id"}});
    requireTrue(openAi.configured && openAi.config.provider == "openai",
                "an OpenAI key must select the complete OpenAI profile");
    requireTrue(openAi.config.endpoint == "https://api.openai.com/v1/chat/completions",
                "an OpenAI key must never retain the DeepSeek endpoint");
    requireTrue(openAi.config.apiKeyHeader == "Authorization" &&
                    openAi.config.apiKeyPrefix == "Bearer ",
                "OpenAI authentication should use a bearer header");

    auto anthropic = resolver.resolve({{"LLM_PROVIDER", "anthropic"},
                                       {"ANTHROPIC_API_KEY", "anthropic-secret"},
                                       {"ANTHROPIC_MODEL", "claude-account-model"},
                                       {"OPENAI_API_KEY", "openai-secret"},
                                       {"OPENAI_MODEL", "ignored-openai-model"}});
    requireTrue(anthropic.config.provider == "anthropic",
                "LLM_PROVIDER should select one explicitly configured credential profile");
    requireTrue(anthropic.config.endpoint == "https://api.anthropic.com/v1/messages",
                "an Anthropic key must select the Anthropic endpoint atomically");
    requireTrue(anthropic.config.apiKeyHeader == "x-api-key" &&
                    anthropic.config.apiKeyPrefix.empty(),
                "a standard Anthropic API key must use x-api-key without Bearer");

    auto workloadToken = resolver.resolve({{"ANTHROPIC_AUTH_TOKEN", "short-lived-token"},
                                           {"ANTHROPIC_MODEL", "claude-workload-model"}});
    requireTrue(workloadToken.config.apiKeyHeader == "Authorization" &&
                    workloadToken.config.apiKeyPrefix == "Bearer ",
                "an Anthropic workload token should use Authorization Bearer");
    requireTrue(resolver.validateConfig(workloadToken.config).ok(),
                "the config validator must accept resolved Anthropic bearer tokens");

    auto deepSeek = resolver.resolve({{"DEEPSEEK_API_KEY", "deepseek-secret"},
                                      {"DEEPSEEK_BASE_URL", "https://proxy.example/v1/"},
                                      {"DEEPSEEK_MODEL", "custom-model"}});
    requireTrue(deepSeek.config.provider == "deepseek" && deepSeek.customEndpoint,
                "an explicit DeepSeek proxy should remain marked as custom");
    requireTrue(deepSeek.config.endpoint == "https://proxy.example/v1/chat/completions" &&
                    deepSeek.config.model == "custom-model",
                "provider overrides should be applied only to their own profile");

    auto empty = resolver.resolve({});
    requireTrue(!empty.configured && empty.config.apiKey.empty() && empty.config.model.empty() &&
                    empty.config.endpoint.empty(),
                "missing credentials must not inject a provider or model default");

    auto missingModel = resolver.resolve({{"OPENAI_API_KEY", "openai-secret"}});
    requireTrue(!missingModel.configured &&
                    missingModel.error.find("OPENAI_MODEL") != std::string::npos &&
                    missingModel.config.endpoint == "https://api.openai.com/v1/chat/completions" &&
                    missingModel.config.apiKey == "openai-secret",
                "a credential must not guess a model but must remain available for discovery");
    auto missingWorkloadModel = resolver.resolve({{"ANTHROPIC_AUTH_TOKEN", "short-lived-token"}});
    requireTrue(!missingWorkloadModel.configured &&
                    missingWorkloadModel.config.apiKeyHeader == "Authorization" &&
                    missingWorkloadModel.config.apiKeyPrefix == "Bearer ",
                "a partial workload-token profile must retain its bearer authentication mode");

    auto ambiguous = resolver.resolve({{"ANTHROPIC_API_KEY", "anthropic-secret"},
                                       {"ANTHROPIC_MODEL", "anthropic-model"},
                                       {"OPENAI_API_KEY", "openai-secret"},
                                       {"OPENAI_MODEL", "openai-model"}});
    requireTrue(!ambiguous.configured && ambiguous.error.find("LLM_PROVIDER") != std::string::npos,
                "multiple credentials must not be resolved by a hard-coded provider priority");

    auto generic = resolver.resolve({{"LLM_API_KEY", "generic-secret"},
                                     {"LLM_BASE_URL", "https://models.example/v1"},
                                     {"LLM_MODEL", "account-specific-model"}});
    requireTrue(generic.configured && generic.config.provider == "openai" &&
                    generic.config.model == "account-specific-model" &&
                    generic.credentialSource == "LLM_API_KEY",
                "generic LLM variables must support arbitrary OpenAI-compatible services");

    auto userAnthropic = resolver.resolveUserProfile("https://gateway.example/v1/messages",
                                                     "organization-model", "user-secret");
    requireTrue(userAnthropic.ok() && userAnthropic.value().config.provider == "anthropic" &&
                    userAnthropic.value().config.apiKeyHeader == "x-api-key",
                "the user endpoint protocol must select Anthropic without a hard-coded model");
    auto userCompatible = resolver.resolveUserProfile("https://gateway.example/v1",
                                                      "qwen-or-any-model", "user-secret");
    requireTrue(userCompatible.ok() && userCompatible.value().config.provider == "openai" &&
                    userCompatible.value().config.endpoint ==
                        "https://gateway.example/v1/chat/completions",
                "a custom OpenAI-compatible endpoint must accept an arbitrary model id");
    auto futureOfficialModel = resolver.resolveUserProfile(
        "https://api.openai.com/v1/chat/completions", "future-account-model", "user-secret");
    requireTrue(futureOfficialModel.ok() &&
                    futureOfficialModel.value().config.model == "future-account-model",
                "local model-name allowlists must not reject provider-side model evolution");
    auto discovery =
        resolver.resolveModelDiscoveryProfile("https://gateway.example/v1", "user-secret");
    requireTrue(
        discovery.ok() && discovery.value().configured && discovery.value().config.model.empty() &&
            discovery.value().config.endpoint == "https://gateway.example/v1/chat/completions",
        "model discovery must resolve protocol and authentication without a model ID");

    cc::AuditResult deterministic;
    deterministic.trustScore.totalScore = 40;
    deterministic.findings.push_back(
        {"RULE-1", cc::Severity::Blocker, "材料缺失", "缺材料", {}, {}, "补充材料"});
    cc::AuditAdvisory proposed;
    proposed.suggestedScore = 180;
    proposed.risks = {{.title = "材料缺失",
                       .severity = cc::Severity::Blocker,
                       .reason = "项目不通过",
                       .ruleIdHint = "WRONG-RULE",
                       .claimIdHint = {},
                       .suggestion = {}},
                      {.title = "材料缺失",
                       .severity = cc::Severity::Info,
                       .reason = "风险很低",
                       .ruleIdHint = "RULE-1",
                       .claimIdHint = {},
                       .suggestion = {}},
                      {.title = "局部结论",
                       .severity = cc::Severity::Warning,
                       .reason = "项目并非无风险，不能通过",
                       .ruleIdHint = {},
                       .claimIdHint = {},
                       .suggestion = {}}};
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
