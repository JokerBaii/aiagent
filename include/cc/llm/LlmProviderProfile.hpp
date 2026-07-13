#pragma once

#include "cc/core/Result.hpp"
#include "cc/llm/LlmTypes.hpp"

#include <map>
#include <string>

namespace cc {

struct LlmProviderProfile {
    LlmConfig config;
    bool configured{false};
    bool customEndpoint{false};
    std::string credentialSource;
    std::string error;
};

class LlmProviderResolver {
  public:
    using Environment = std::map<std::string, std::string, std::less<>>;

    [[nodiscard]] LlmProviderProfile resolve(const Environment& environment) const;

    [[nodiscard]] Result<LlmProviderProfile>
    resolveUserProfile(std::string endpoint, std::string model, std::string apiKey) const;

    /** @brief 仅解析 endpoint 与认证，用于在尚未选择模型时读取 provider 模型目录。 */
    [[nodiscard]] Result<LlmProviderProfile> resolveModelDiscoveryProfile(std::string endpoint,
                                                                          std::string apiKey) const;

    [[nodiscard]] Result<void> validateConfig(const LlmConfig& config) const;
};

} // namespace cc
