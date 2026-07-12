#include "cc/llm/LlmProviderProfile.hpp"

#include "cc/llm/EndpointParser.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace cc {
namespace {

enum class ProviderKind { Anthropic, OpenAi, DeepSeek };

struct NormalizedEndpoint {
    ProviderKind provider{ProviderKind::OpenAi};
    std::string url;
    std::string host;
};

[[nodiscard]] std::string trim(std::string text) {
    const auto whitespace = [](unsigned char character) { return std::isspace(character) != 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [&](char character) {
                   return !whitespace(static_cast<unsigned char>(character));
               }));
    text.erase(std::find_if(text.rbegin(), text.rend(),
                            [&](char character) {
                                return !whitespace(static_cast<unsigned char>(character));
                            })
                   .base(),
               text.end());
    return text;
}

[[nodiscard]] std::string value(const LlmProviderResolver::Environment& environment,
                                std::string_view name) {
    const auto item = environment.find(name);
    return item == environment.end() ? std::string{} : trim(item->second);
}

[[nodiscard]] bool hasUnsafeText(std::string_view text) {
    return std::any_of(text.begin(), text.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte == 0U || byte == 127U || byte < 32U;
    });
}

[[nodiscard]] std::string providerName(ProviderKind provider) {
    switch (provider) {
    case ProviderKind::Anthropic:
        return "anthropic";
    case ProviderKind::OpenAi:
        return "openai";
    case ProviderKind::DeepSeek:
        return "deepseek";
    }
    return "openai";
}

[[nodiscard]] bool isAnthropicHost(std::string_view host) {
    return util::lowerAscii(std::string{host}) == "api.anthropic.com";
}

[[nodiscard]] bool isOpenAiHost(std::string_view host) {
    return util::lowerAscii(std::string{host}) == "api.openai.com";
}

[[nodiscard]] bool isDeepSeekHost(std::string_view host) {
    return util::lowerAscii(std::string{host}) == "api.deepseek.com";
}

[[nodiscard]] bool endsWithPathSegment(std::string_view path, std::string_view suffix) {
    return path == suffix || (path.size() > suffix.size() && path.ends_with(suffix) &&
                              path[path.size() - suffix.size() - 1U] == '/');
}

[[nodiscard]] std::string stripTrailingSlashes(std::string path) {
    while (path.size() > 1U && path.ends_with('/')) {
        path.pop_back();
    }
    return path;
}

[[nodiscard]] ProviderKind providerFromModel(std::string_view model) {
    const auto lower = util::lowerAscii(std::string{model});
    if (lower.starts_with("claude-")) {
        return ProviderKind::Anthropic;
    }
    if (lower.starts_with("deepseek-")) {
        return ProviderKind::DeepSeek;
    }
    return ProviderKind::OpenAi;
}

[[nodiscard]] Result<void> validateModel(ProviderKind provider, std::string_view host,
                                         std::string_view model) {
    if (model.empty() || model.size() > 256U || hasUnsafeText(model)) {
        return Result<void>::failure("LLM model 为空、过长或包含控制字符");
    }
    const auto lower = util::lowerAscii(std::string{model});
    if (isAnthropicHost(host) && !lower.starts_with("claude-")) {
        return Result<void>::failure("Anthropic 官方 endpoint 只能配置 Claude 模型");
    }
    if (isDeepSeekHost(host) && !lower.starts_with("deepseek-")) {
        return Result<void>::failure("DeepSeek 官方 endpoint 只能配置 DeepSeek 模型");
    }
    if (isOpenAiHost(host) && (lower.starts_with("claude-") || lower.starts_with("deepseek-"))) {
        return Result<void>::failure("OpenAI 官方 endpoint 与当前模型不一致");
    }
    if (provider == ProviderKind::Anthropic &&
        (lower.starts_with("gpt-") || lower.starts_with("deepseek-") || lower.starts_with("o1") ||
         lower.starts_with("o3") || lower.starts_with("o4"))) {
        return Result<void>::failure("Anthropic Messages endpoint 与当前模型不一致");
    }
    return Result<void>::success();
}

[[nodiscard]] Result<NormalizedEndpoint>
normalizeEndpoint(std::string endpoint, std::string_view model,
                  const ProviderKind* requiredProvider = nullptr) {
    endpoint = trim(std::move(endpoint));
    while (endpoint.size() > std::string_view{"https://"}.size() && endpoint.ends_with('/')) {
        endpoint.pop_back();
    }
    auto parsed = EndpointParser{}.parse(endpoint);
    if (!parsed.ok()) {
        return Result<NormalizedEndpoint>::failure(parsed.error());
    }
    if (parsed.value().target.find('?') != std::string::npos) {
        return Result<NormalizedEndpoint>::failure("LLM endpoint 不允许包含 query 参数");
    }

    auto path = stripTrailingSlashes(parsed.value().target);
    const bool messages = endsWithPathSegment(path, "messages");
    const bool chatCompletions = endsWithPathSegment(path, "chat/completions");
    if (path.find("/messages") != std::string::npos && !messages) {
        return Result<NormalizedEndpoint>::failure("Anthropic endpoint 必须以 /messages 结尾");
    }
    if (path.find("/chat/completions") != std::string::npos && !chatCompletions) {
        return Result<NormalizedEndpoint>::failure(
            "OpenAI-compatible endpoint 必须以 /chat/completions 结尾");
    }

    ProviderKind provider = ProviderKind::OpenAi;
    if (requiredProvider != nullptr) {
        if ((*requiredProvider == ProviderKind::Anthropic && chatCompletions) ||
            (*requiredProvider != ProviderKind::Anthropic && messages)) {
            const auto expected =
                *requiredProvider == ProviderKind::Anthropic ? "/messages" : "/chat/completions";
            return Result<NormalizedEndpoint>::failure("当前 provider 的 endpoint 必须以 " +
                                                       std::string{expected} + " 结尾");
        }
        provider = *requiredProvider;
    } else if (messages) {
        provider = ProviderKind::Anthropic;
    } else if (chatCompletions) {
        provider =
            isDeepSeekHost(parsed.value().host) ? ProviderKind::DeepSeek : ProviderKind::OpenAi;
    } else if (isAnthropicHost(parsed.value().host)) {
        provider = ProviderKind::Anthropic;
    } else if (isDeepSeekHost(parsed.value().host)) {
        provider = ProviderKind::DeepSeek;
    } else if (isOpenAiHost(parsed.value().host)) {
        provider = ProviderKind::OpenAi;
    } else {
        provider = providerFromModel(model);
    }

    if ((isAnthropicHost(parsed.value().host) && provider != ProviderKind::Anthropic) ||
        ((isOpenAiHost(parsed.value().host) || isDeepSeekHost(parsed.value().host)) &&
         provider == ProviderKind::Anthropic)) {
        return Result<NormalizedEndpoint>::failure("LLM endpoint 主机与请求协议不一致");
    }
    if (!messages && !chatCompletions) {
        if (path == "/") {
            path.clear();
        }
        const auto suffix =
            provider == ProviderKind::Anthropic
                ? (path.ends_with("/v1") ? "/messages" : "/v1/messages")
            : provider == ProviderKind::DeepSeek
                ? "/chat/completions"
                : (path.ends_with("/v1") ? "/chat/completions" : "/v1/chat/completions");
        path += suffix;
    }

    const auto expectedMessages = provider == ProviderKind::Anthropic;
    if (expectedMessages != endsWithPathSegment(path, "messages") ||
        (!expectedMessages && !endsWithPathSegment(path, "chat/completions"))) {
        return Result<NormalizedEndpoint>::failure(
            expectedMessages ? "Anthropic endpoint 必须以 /messages 结尾"
                             : "OpenAI-compatible endpoint 必须以 /chat/completions 结尾");
    }

    auto normalized = std::string{"https://"} + parsed.value().hostHeader + path;
    return Result<NormalizedEndpoint>::success(
        {.provider = provider, .url = std::move(normalized), .host = parsed.value().host});
}

[[nodiscard]] LlmProviderProfile profileFor(NormalizedEndpoint normalized, std::string model,
                                            std::string key, std::string credentialSource) {
    LlmProviderProfile profile;
    profile.config.provider = providerName(normalized.provider);
    profile.config.endpoint = std::move(normalized.url);
    profile.config.model = trim(std::move(model));
    profile.config.apiKey = std::move(key);
    profile.config.apiKeyHeader =
        normalized.provider == ProviderKind::Anthropic ? "x-api-key" : "Authorization";
    profile.config.apiKeyPrefix = normalized.provider == ProviderKind::Anthropic ? "" : "Bearer ";
    profile.configured = !profile.config.apiKey.empty();
    profile.credentialSource = std::move(credentialSource);
    profile.customEndpoint =
        profile.config.endpoint != "https://api.anthropic.com/v1/messages" &&
        profile.config.endpoint != "https://api.openai.com/v1/chat/completions" &&
        profile.config.endpoint != "https://api.deepseek.com/chat/completions";
    return profile;
}

[[nodiscard]] LlmProviderProfile invalidProfile(std::string error) {
    LlmProviderProfile profile;
    profile.config.apiKey.clear();
    profile.configured = false;
    profile.error = std::move(error);
    return profile;
}

[[nodiscard]] LlmProviderProfile
environmentProfile(const LlmProviderResolver::Environment& environment, ProviderKind provider,
                   std::string key, std::string credentialSource, std::string_view baseName,
                   std::string_view modelName, std::string defaultEndpoint,
                   std::string defaultModel) {
    if (key.size() > 8192U || hasUnsafeText(key)) {
        return invalidProfile(std::string{credentialSource} +
                              " 配置无效: API key 过长或包含控制字符");
    }
    auto endpoint = value(environment, baseName);
    if (endpoint.empty()) {
        endpoint = std::move(defaultEndpoint);
    }
    auto model = value(environment, modelName);
    if (model.empty()) {
        model = std::move(defaultModel);
    }
    auto normalized = normalizeEndpoint(endpoint, model, &provider);
    if (!normalized.ok()) {
        return invalidProfile(std::string{baseName} + " 配置无效: " + normalized.error());
    }
    auto modelValidation = validateModel(provider, normalized.value().host, model);
    if (!modelValidation.ok()) {
        return invalidProfile(std::string{modelName} + " 配置无效: " + modelValidation.error());
    }
    auto profile = profileFor(std::move(normalized.value()), std::move(model), std::move(key),
                              std::move(credentialSource));
    if (profile.credentialSource == "ANTHROPIC_AUTH_TOKEN") {
        profile.config.apiKeyHeader = "Authorization";
        profile.config.apiKeyPrefix = "Bearer ";
    }
    profile.configured = profile.config.apiKey.size() >= 8U;
    return profile;
}

} // namespace

LlmProviderProfile
LlmProviderResolver::resolve(const LlmProviderResolver::Environment& environment) const {
    const auto anthropicKey = value(environment, "ANTHROPIC_API_KEY");
    if (!anthropicKey.empty()) {
        return environmentProfile(environment, ProviderKind::Anthropic, anthropicKey,
                                  "ANTHROPIC_API_KEY", "ANTHROPIC_BASE_URL", "ANTHROPIC_MODEL",
                                  "https://api.anthropic.com/v1/messages", "claude-sonnet-4-6");
    }
    const auto anthropicToken = value(environment, "ANTHROPIC_AUTH_TOKEN");
    if (!anthropicToken.empty()) {
        return environmentProfile(environment, ProviderKind::Anthropic, anthropicToken,
                                  "ANTHROPIC_AUTH_TOKEN", "ANTHROPIC_BASE_URL", "ANTHROPIC_MODEL",
                                  "https://api.anthropic.com/v1/messages", "claude-sonnet-4-6");
    }
    const auto openAiKey = value(environment, "OPENAI_API_KEY");
    if (!openAiKey.empty()) {
        return environmentProfile(environment, ProviderKind::OpenAi, openAiKey, "OPENAI_API_KEY",
                                  "OPENAI_BASE_URL", "OPENAI_MODEL",
                                  "https://api.openai.com/v1/chat/completions", "gpt-4o-mini");
    }

    auto deepSeekKey = value(environment, "DEEPSEEK_API_KEY");
    auto credentialSource = std::string{"DEEPSEEK_API_KEY"};
    if (deepSeekKey.empty()) {
        deepSeekKey = value(environment, "DEEPSEEK_AUTH_TOKEN");
        credentialSource = "DEEPSEEK_AUTH_TOKEN";
    }
    return environmentProfile(environment, ProviderKind::DeepSeek, std::move(deepSeekKey),
                              std::move(credentialSource), "DEEPSEEK_BASE_URL", "DEEPSEEK_MODEL",
                              "https://api.deepseek.com/chat/completions", "deepseek-v4-flash");
}

Result<LlmProviderProfile> LlmProviderResolver::resolveUserProfile(std::string endpoint,
                                                                   std::string model,
                                                                   std::string apiKey) const {
    model = trim(std::move(model));
    apiKey = trim(std::move(apiKey));
    if (apiKey.size() > 8192U || hasUnsafeText(apiKey)) {
        return Result<LlmProviderProfile>::failure("LLM API key 过长或包含控制字符");
    }
    auto normalized = normalizeEndpoint(std::move(endpoint), model);
    if (!normalized.ok()) {
        return Result<LlmProviderProfile>::failure(normalized.error());
    }
    auto modelValidation =
        validateModel(normalized.value().provider, normalized.value().host, model);
    if (!modelValidation.ok()) {
        return Result<LlmProviderProfile>::failure(modelValidation.error());
    }
    auto profile =
        profileFor(std::move(normalized.value()), std::move(model), std::move(apiKey), "user");
    profile.configured = profile.config.apiKey.size() >= 8U;
    return Result<LlmProviderProfile>::success(std::move(profile));
}

Result<void> LlmProviderResolver::validateConfig(const LlmConfig& config) const {
    const auto provider = util::lowerAscii(trim(config.provider));
    ProviderKind requiredProvider;
    if (provider == "anthropic") {
        requiredProvider = ProviderKind::Anthropic;
    } else if (provider == "openai") {
        requiredProvider = ProviderKind::OpenAi;
    } else if (provider == "deepseek") {
        requiredProvider = ProviderKind::DeepSeek;
    } else {
        return Result<void>::failure("不支持的 LLM provider: " + provider);
    }

    auto normalized = normalizeEndpoint(config.endpoint, config.model, &requiredProvider);
    if (!normalized.ok()) {
        return Result<void>::failure(normalized.error());
    }
    if (normalized.value().url != trim(config.endpoint)) {
        return Result<void>::failure(requiredProvider == ProviderKind::Anthropic
                                         ? "Anthropic endpoint 必须是完整的 /messages 地址"
                                         : "OpenAI-compatible endpoint 必须是完整的 "
                                           "/chat/completions 地址");
    }
    auto modelValidation = validateModel(requiredProvider, normalized.value().host, config.model);
    if (!modelValidation.ok()) {
        return modelValidation;
    }
    if (requiredProvider == ProviderKind::Anthropic) {
        if (config.apiKeyHeader != "x-api-key" || !config.apiKeyPrefix.empty()) {
            return Result<void>::failure("Anthropic 认证必须使用 x-api-key，且不能添加 Bearer");
        }
    } else if (config.apiKeyHeader != "Authorization" || config.apiKeyPrefix != "Bearer ") {
        return Result<void>::failure("OpenAI-compatible 认证必须使用 Authorization: Bearer");
    }
    return Result<void>::success();
}

} // namespace cc
