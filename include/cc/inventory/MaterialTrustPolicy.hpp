#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

[[nodiscard]] const ProjectAsset* findAsset(const ProjectInventory& inventory,
                                            const std::filesystem::path& relativePath);
[[nodiscard]] bool isReliableTextDocument(const TextDocument& document);
[[nodiscard]] bool needsTextReview(const TextDocument& document);
[[nodiscard]] bool isExcludedTruthPath(const std::filesystem::path& path);
[[nodiscard]] bool isTrustedNarrativeAsset(const ProjectAsset& asset);
[[nodiscard]] bool isTrustedClaimSource(const ProjectAsset& asset);

} // namespace cc
