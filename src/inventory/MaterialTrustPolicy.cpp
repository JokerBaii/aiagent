#include "cc/inventory/MaterialTrustPolicy.hpp"

#include "cc/util/StringUtil.hpp"

#include <array>

namespace cc {
namespace {

[[nodiscard]] bool excludedComponent(const std::string& component) {
    constexpr std::array<const char*, 18> excluded = {
        ".git",  ".workspaces", "node_modules", "vendor", "third_party", "external",
        "build", "dist",        "out",          "target", "generated",   "__pycache__",
        ".next", "coverage",    "test",         "tests",  "mock",        "mocks",
    };
    for (const auto* value : excluded) {
        if (component == value) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isLockOrStyleFile(const std::filesystem::path& path) {
    const auto name = util::lowerAscii(path.filename().string());
    const auto extension = util::lowerAscii(path.extension().string());
    if (extension == ".css" || extension == ".scss" || extension == ".less" ||
        extension == ".map" || extension == ".lock") {
        return true;
    }
    return name == "package-lock.json" || name == "pnpm-lock.yaml" || name == "yarn.lock" ||
           name == "cargo.lock" || name == "poetry.lock" || name == "composer.lock";
}

} // namespace

const ProjectAsset* findAsset(const ProjectInventory& inventory,
                              const std::filesystem::path& relativePath) {
    const auto expected = relativePath.lexically_normal().generic_string();
    for (const auto& asset : inventory.assets) {
        if (asset.relativePath.lexically_normal().generic_string() == expected) {
            return &asset;
        }
    }
    return nullptr;
}

bool needsTextReview(const TextDocument& document) {
    const auto status = util::lowerAscii(document.status);
    return status.empty() || util::contains(status, "need_review") ||
           util::contains(status, "truncated") || util::contains(status, "parse_failed") ||
           util::contains(status, "empty") || util::contains(status, "unreadable") ||
           util::contains(status, "limited");
}

bool isReliableTextDocument(const TextDocument& document) {
    const auto status = util::lowerAscii(document.status);
    return !needsTextReview(document) && status.rfind("extracted", 0U) == 0U;
}

bool isExcludedTruthPath(const std::filesystem::path& path) {
    for (const auto& component : path) {
        if (excludedComponent(util::lowerAscii(component.string()))) {
            return true;
        }
    }
    const auto name = util::lowerAscii(path.filename().string());
    if (util::contains(name, ".mock.") || util::contains(name, "_mock.") ||
        util::contains(name, ".test.") || util::contains(name, "_test.")) {
        return true;
    }
    return isLockOrStyleFile(path);
}

bool isTrustedNarrativeAsset(const ProjectAsset& asset) {
    if (!asset.auditable || asset.sensitive || asset.generated || asset.vendored ||
        isExcludedTruthPath(asset.relativePath)) {
        return false;
    }
    switch (asset.role) {
    case AssetRole::ProjectDeclaration:
    case AssetRole::BusinessPlan:
    case AssetRole::PitchDeck:
    case AssetRole::MarketResearch:
    case AssetRole::CompetitorAnalysis:
    case AssetRole::FinancialPlan:
    case AssetRole::UserResearch:
    case AssetRole::DeploymentDoc:
    case AssetRole::ResearchPaper:
    case AssetRole::SocialPracticeProof:
        return true;
    default:
        return false;
    }
}

bool isTrustedClaimSource(const ProjectAsset& asset) {
    if (!isTrustedNarrativeAsset(asset)) {
        return false;
    }
    return asset.role != AssetRole::CompetitorAnalysis && asset.role != AssetRole::UserResearch;
}

} // namespace cc
