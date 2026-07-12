/**
 * @file LlmPromptGuard.hpp
 * @brief 发往外部模型前的上下文边界与密钥脱敏。
 */

#pragma once

#include "cc/llm/LlmTypes.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace cc {

struct LlmPromptBudget {
    std::size_t maxMessages{32U};
    std::size_t maxMessageBytes{std::size_t{16U} * 1024U};
    std::size_t maxTotalBytes{std::size_t{96U} * 1024U};
};

class LlmPromptGuard {
  public:
    [[nodiscard]] std::string redactSecrets(std::string text) const;
    [[nodiscard]] std::vector<LlmMessage> sanitize(const std::vector<LlmMessage>& messages,
                                                   const LlmPromptBudget& budget = {}) const;
};

} // namespace cc
