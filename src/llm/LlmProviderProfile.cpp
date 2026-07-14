#include "cc/llm/LlmProviderProfile.hpp"

#include "cc/llm/EndpointParser.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace cc {
namespace {

constexpr std::string_view kDefaultEndpoint{"https://api.deepseek.com/chat/completions"};

[[nodiscard]] std::string trim(std::string text) {
    const auto whitespace = [](unsigned char character) { return std::isspace(character) != 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [&](char character) {
                   return !whitespace(static_cast<unsigned char>(character));
               }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [&](char character) {
                   return !whitespace(static_cast<unsigned char>(character));
               }).base(),
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

[[nodiscard]] Result<void> validateModel(std::string_view model) {
    if (model.empty() || model.size() > 256U || hasUnsafeText(model)) {
        return Result<void>::failure("DeepSeek model 为空、过长或包含控制字符");
    }
    return Result<void>::success();
}

[[nodiscard]] std::string stripTrailingSlashes(std::string path) {
    while (path.size() > 1U && path.ends_with('/')) {
        path.pop_back();
    }
    return path;
}

[[nodiscard]] bool endsWithChatCompletions(std::string_view path) {
    constexpr std::string_view suffix{"chat/completions"};
    return path == suffix || (path.size() > suffix.size() && path.ends_with(suffix) &&
                              path[path.size() - suffix.size() - 1U] == '/');
}

[[nodiscard]] Result<std::string> normalizeEndpoint(std::string endpoint) {
    endpoint = trim(std::move(endpoint));
    while (endpoint.size() > std::string_view{"https://"}.size() && endpoint.ends_with('/')) {
        endpoint.pop_back();
    }
    auto parsed = EndpointParser{}.parse(endpoint);
    if (!parsed.ok()) {
        return Result<std::string>::failure(parsed.error());
    }
    if (parsed.value().target.find('?') != std::string::npos) {
        return Result<std::string>::failure("DeepSeek endpoint 不允许包含 query 参数");
    }
    auto path = stripTrailingSlashes(parsed.value().target);
    if (path.find("/messages") != std::string::npos) {
        return Result<std::string>::failure("DeepSeek 只支持 /chat/completions 协议");
    }
    if (path.find("/chat/completions") != std::string::npos && !endsWithChatCompletions(path)) {
        return Result<std::string>::failure("DeepSeek endpoint 必须以 /chat/completions 结尾");
    }
    if (!endsWithChatCompletions(path)) {
        if (path == "/") {
            path.clear();
        }
        path += "/chat/completions";
    }
    return Result<std::string>::success("https://" + parsed.value().hostHeader + path);
}

[[nodiscard]] LlmProviderProfile invalidProfile(std::string error) {
    LlmProviderProfile profile;
    profile.configured = false;
    profile.error = std::move(error);
    return profile;
}

[[nodiscard]] LlmProviderProfile profileFor(std::string endpoint, std::string model,
                                            std::string key, std::string source) {
    LlmProviderProfile profile;
    profile.config.provider = "deepseek";
    profile.config.endpoint = std::move(endpoint);
    profile.config.model = trim(std::move(model));
    profile.config.apiKey = std::move(key);
    profile.config.apiKeyHeader = "Authorization";
    profile.config.apiKeyPrefix = "Bearer ";
    profile.configured = profile.config.apiKey.size() >= 8U && !profile.config.model.empty();
    profile.customEndpoint = profile.config.endpoint != kDefaultEndpoint;
    profile.credentialSource = std::move(source);
    return profile;
}

} // namespace

LlmProviderProfile
LlmProviderResolver::resolve(const LlmProviderResolver::Environment& environment) const {
    const auto selectedProvider = util::lowerAscii(value(environment, "LLM_PROVIDER"));
    if (!selectedProvider.empty() && selectedProvider != "deepseek") {
        return invalidProfile("本应用只支持 LLM_PROVIDER=deepseek");
    }

    auto key = value(environment, "DEEPSEEK_API_KEY");
    const auto authToken = value(environment, "DEEPSEEK_AUTH_TOKEN");
    if (!key.empty() && !authToken.empty()) {
        return invalidProfile("DEEPSEEK_API_KEY 与 DEEPSEEK_AUTH_TOKEN 不能同时配置");
    }
    auto source = std::string{"DEEPSEEK_API_KEY"};
    if (key.empty()) {
        key = authToken;
        source = "DEEPSEEK_AUTH_TOKEN";
    }
    if (key.empty()) {
        return {};
    }
    if (key.size() > 8192U || hasUnsafeText(key)) {
        return invalidProfile(source + " 配置无效: 密钥过长或包含控制字符");
    }

    auto endpoint = value(environment, "DEEPSEEK_BASE_URL");
    if (endpoint.empty()) {
        endpoint = std::string{kDefaultEndpoint};
    }
    auto normalized = normalizeEndpoint(std::move(endpoint));
    if (!normalized.ok()) {
        return invalidProfile("DEEPSEEK_BASE_URL 配置无效: " + normalized.error());
    }
    auto model = value(environment, "DEEPSEEK_MODEL");
    auto profile = profileFor(std::move(normalized.value()), model, std::move(key), source);
    if (model.empty()) {
        profile.configured = false;
        profile.error = "DEEPSEEK_MODEL 未配置；可在界面按当前凭证获取模型列表后选择";
        return profile;
    }
    auto validModel = validateModel(model);
    if (!validModel.ok()) {
        return invalidProfile("DEEPSEEK_MODEL 配置无效: " + validModel.error());
    }
    return profile;
}

Result<LlmProviderProfile> LlmProviderResolver::resolveUserProfile(std::string endpoint,
                                                                   std::string model,
                                                                   std::string apiKey) const {
    model = trim(std::move(model));
    auto validModel = validateModel(model);
    if (!validModel.ok()) {
        return Result<LlmProviderProfile>::failure(validModel.error());
    }
    auto discovery = resolveModelDiscoveryProfile(std::move(endpoint), std::move(apiKey));
    if (!discovery.ok()) {
        return discovery;
    }
    auto profile = std::move(discovery.value());
    profile.config.model = std::move(model);
    profile.configured = profile.config.apiKey.size() >= 8U;
    return Result<LlmProviderProfile>::success(std::move(profile));
}

Result<LlmProviderProfile>
LlmProviderResolver::resolveModelDiscoveryProfile(std::string endpoint, std::string apiKey) const {
    apiKey = trim(std::move(apiKey));
    if (apiKey.size() < 8U || apiKey.size() > 8192U || hasUnsafeText(apiKey)) {
        return Result<LlmProviderProfile>::failure("DeepSeek API key 长度或格式无效");
    }
    auto normalized = normalizeEndpoint(std::move(endpoint));
    if (!normalized.ok()) {
        return Result<LlmProviderProfile>::failure(normalized.error());
    }
    auto profile = profileFor(std::move(normalized.value()), {}, std::move(apiKey), "user");
    profile.configured = true;
    return Result<LlmProviderProfile>::success(std::move(profile));
}

Result<void> LlmProviderResolver::validateConfig(const LlmConfig& config) const {
    if (util::lowerAscii(trim(config.provider)) != "deepseek") {
        return Result<void>::failure("本应用只支持 DeepSeek provider");
    }
    if (config.apiKey.size() < 8U || config.apiKey.size() > 8192U ||
        hasUnsafeText(config.apiKey)) {
        return Result<void>::failure("DeepSeek API key 长度或格式无效");
    }
    auto normalized = normalizeEndpoint(config.endpoint);
    if (!normalized.ok()) {
        return Result<void>::failure(normalized.error());
    }
    if (normalized.value() != trim(config.endpoint)) {
        return Result<void>::failure("DeepSeek endpoint 必须是完整的 /chat/completions 地址");
    }
    auto validModel = validateModel(config.model);
    if (!validModel.ok()) {
        return validModel;
    }
    if (config.apiKeyHeader != "Authorization" || config.apiKeyPrefix != "Bearer ") {
        return Result<void>::failure("DeepSeek 认证必须使用 Authorization: Bearer");
    }
    return Result<void>::success();
}

} // namespace cc
