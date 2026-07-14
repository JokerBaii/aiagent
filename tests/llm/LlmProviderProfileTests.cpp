#include "../TestSupport.hpp"
#include "cc/llm/LlmProviderProfile.hpp"

void runLlmProviderProfileTests() {
    cc::LlmProviderResolver resolver;

    auto deepSeek = resolver.resolve({{"DEEPSEEK_API_KEY", "deepseek-secret"},
                                      {"DEEPSEEK_BASE_URL", "https://proxy.example/v1/"},
                                      {"DEEPSEEK_MODEL", "custom-model"}});
    requireTrue(deepSeek.configured && deepSeek.config.provider == "deepseek" &&
                    deepSeek.customEndpoint,
                "a DeepSeek proxy profile should be configured atomically");
    requireTrue(deepSeek.config.endpoint == "https://proxy.example/v1/chat/completions" &&
                    deepSeek.config.model == "custom-model" &&
                    deepSeek.config.apiKeyHeader == "Authorization" &&
                    deepSeek.config.apiKeyPrefix == "Bearer ",
                "DeepSeek profiles must use the native chat-completions endpoint and bearer auth");
    requireTrue(resolver.validateConfig(deepSeek.config).ok(),
                "the resolved DeepSeek profile should pass strict validation");

    auto official = resolver.resolve({{"LLM_PROVIDER", "deepseek"},
                                      {"DEEPSEEK_API_KEY", "deepseek-secret"},
                                      {"DEEPSEEK_MODEL", "deepseek-chat"}});
    requireTrue(official.config.endpoint == "https://api.deepseek.com/chat/completions" &&
                    !official.customEndpoint,
                "the official DeepSeek endpoint should be the only default");

    auto missingModel = resolver.resolve({{"DEEPSEEK_API_KEY", "deepseek-secret"}});
    requireTrue(!missingModel.configured &&
                    missingModel.error.find("DEEPSEEK_MODEL") != std::string::npos &&
                    missingModel.config.apiKey == "deepseek-secret",
                "a DeepSeek credential should remain available for model discovery");

    auto unsupported = resolver.resolve({{"LLM_PROVIDER", "unsupported"},
                                         {"DEEPSEEK_API_KEY", "deepseek-secret"},
                                         {"DEEPSEEK_MODEL", "deepseek-chat"}});
    requireTrue(!unsupported.configured && unsupported.error.find("只支持") != std::string::npos,
                "non-DeepSeek provider selection must be rejected");

    auto ambiguousCredential =
        resolver.resolve({{"DEEPSEEK_API_KEY", "deepseek-secret"},
                          {"DEEPSEEK_AUTH_TOKEN", "deepseek-token"},
                          {"DEEPSEEK_MODEL", "deepseek-chat"}});
    requireTrue(!ambiguousCredential.configured &&
                    ambiguousCredential.error.find("不能同时配置") != std::string::npos,
                "ambiguous DeepSeek credentials must fail closed");

    auto empty = resolver.resolve({});
    requireTrue(!empty.configured && empty.config.apiKey.empty() && empty.config.endpoint.empty(),
                "missing DeepSeek credentials must leave the profile empty");

    auto userProfile = resolver.resolveUserProfile("https://gateway.example/v1", "future-model",
                                                   "user-secret");
    requireTrue(userProfile.ok() && userProfile.value().config.provider == "deepseek" &&
                    userProfile.value().config.endpoint ==
                        "https://gateway.example/v1/chat/completions" &&
                    userProfile.value().config.model == "future-model",
                "custom DeepSeek-compatible gateways should accept provider-side model evolution");

    auto messagesEndpoint = resolver.resolveUserProfile("https://gateway.example/v1/messages",
                                                         "deepseek-model", "user-secret");
    requireTrue(!messagesEndpoint.ok(), "non-DeepSeek /messages endpoints must not be accepted");

    auto discovery =
        resolver.resolveModelDiscoveryProfile("https://api.deepseek.com", "user-secret");
    requireTrue(discovery.ok() && discovery.value().configured &&
                    discovery.value().config.model.empty() &&
                    discovery.value().config.endpoint ==
                        "https://api.deepseek.com/chat/completions",
                "model discovery should normalize only the DeepSeek protocol");
}
