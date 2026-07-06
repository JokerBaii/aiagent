/**
 * @file ContextCompactor.hpp
 * @brief 审计上下文摘要。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

class ContextCompactor {
  public:
    [[nodiscard]] std::string summarize(const ProjectInventory& inventory,
                                        const std::vector<TextDocument>& corpus) const;
};

} // namespace cc
