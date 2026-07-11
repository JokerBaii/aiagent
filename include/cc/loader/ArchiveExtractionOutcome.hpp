#pragma once

#include "cc/core/ProjectModels.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace cc {

struct ArchiveExtractionOutcome {
    std::vector<std::filesystem::path> files;
    std::vector<DeferredInputFile> deferredFiles;
    std::vector<std::string> warnings;
    std::size_t omittedFileCount{0};
};

} // namespace cc
