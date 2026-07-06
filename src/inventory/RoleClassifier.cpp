/**
 * @file RoleClassifier.cpp
 * @brief 项目资产语义角色分类实现。
 */

#include "cc/inventory/RoleClassifier.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

AssetRole RoleClassifier::classify(const ProjectAsset& asset) const {
    const auto name = util::lowerAscii(asset.fileName);
    const auto path = util::lowerAscii(asset.relativePath.generic_string());
    const auto original = asset.relativePath.generic_string();
    if (asset.sensitive) {
        return AssetRole::SecretRisk;
    }
    if (asset.generated) {
        return AssetRole::Generated;
    }
    if (asset.vendored) {
        return AssetRole::Vendored;
    }
    if (ArchiveExtractor::isArchivePath(asset.relativePath)) {
        return AssetRole::Archive;
    }
    if (name == "cmakelists.txt" || name == "makefile" || name == "package.json" ||
        name == "pyproject.toml" || name == "pom.xml" || name == "build.gradle") {
        return AssetRole::BuildSystem;
    }
    if (name == "requirements.txt" || name == "package-lock.json" || name == "pnpm-lock.yaml" ||
        name == "yarn.lock" || name == "cargo.toml" || name == "go.mod") {
        return AssetRole::DependencyManifest;
    }
    if (asset.language == "cpp" || asset.language == "hpp" || asset.language == "py" ||
        asset.language == "js" || asset.language == "ts" || asset.language == "qml") {
        return AssetRole::SourceCode;
    }
    if (util::contains(original, "申报") || util::contains(path, "declaration")) {
        return AssetRole::ProjectDeclaration;
    }
    if (util::contains(original, "商业计划") || util::contains(path, "business_plan")) {
        return AssetRole::BusinessPlan;
    }
    if (util::contains(original, "路演") || util::contains(path, "pitch") ||
        asset.extension == ".pptx") {
        return AssetRole::PitchDeck;
    }
    if (util::contains(original, "市场") || util::contains(original, "调研") ||
        util::contains(path, "market")) {
        return AssetRole::MarketResearch;
    }
    if (util::contains(original, "竞品") || util::contains(path, "competitor")) {
        return AssetRole::CompetitorAnalysis;
    }
    if (util::contains(original, "财务") || util::contains(path, "financial")) {
        return AssetRole::FinancialPlan;
    }
    if (util::contains(original, "用户") || util::contains(original, "问卷") ||
        util::contains(original, "访谈") || util::contains(path, "survey")) {
        return AssetRole::UserResearch;
    }
    if (util::contains(original, "部署") || util::contains(path, "deploy")) {
        return AssetRole::DeploymentDoc;
    }
    if (util::contains(original, "实验") || util::contains(path, "baseline") ||
        asset.extension == ".csv") {
        return AssetRole::ExperimentData;
    }
    if (util::contains(original, "论文") || util::contains(path, "paper")) {
        return AssetRole::ResearchPaper;
    }
    if (util::contains(original, "专利") || util::contains(original, "软著")) {
        return AssetRole::PatentCopyright;
    }
    if (util::contains(original, "社会实践") || util::contains(original, "活动")) {
        return AssetRole::SocialPracticeProof;
    }
    if (util::contains(original, "证明") || util::contains(original, "成果")) {
        return AssetRole::ProofMaterial;
    }
    if (asset.extension == ".png" || asset.extension == ".jpg" || asset.extension == ".mp4") {
        return AssetRole::ResourceAsset;
    }
    return AssetRole::Unknown;
}

} // namespace cc
