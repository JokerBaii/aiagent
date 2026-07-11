#pragma once

#include "cc/core/Enums.hpp"
#include "cc/core/Result.hpp"

#include <filesystem>
#include <string>

namespace cc {

class ProjectMemory {
  public:
    [[nodiscard]] Result<void> init(const std::filesystem::path& workspaceRoot,
                                    CompetitionType track) const;
    [[nodiscard]] Result<std::string> loadInstructions(
        const std::filesystem::path& workspaceRoot) const;
};

} // namespace cc
