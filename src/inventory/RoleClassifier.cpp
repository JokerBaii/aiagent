/**
 * @file RoleClassifier.cpp
 * @brief 项目资产语义角色分类实现。
 */

#include "cc/inventory/RoleClassifier.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/loader/ArchiveExtractor.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {
namespace {

[[nodiscard]] bool isArchiveMime(const std::string& mime) {
    return mime == "application/archive" || mime == "application/zip" ||
           mime == "application/gzip" || mime == "application/zstd" ||
           mime == "application/x-7z-compressed" || mime == "application/vnd.rar" ||
           mime == "application/x-tar" || mime == "application/x-bzip2" ||
           mime == "application/x-xz" || mime == "application/x-cpio" ||
           mime == "application/x-archive" || mime == "application/vnd.ms-cab-compressed" ||
           mime == "application/x-lz4" || mime == "application/x-iso9660-image" ||
           mime == "application/x-rpm" || mime == "application/vnd.debian.binary-package" ||
           mime == "application/vnd.android.package-archive" || mime == "application/java-archive";
}

[[nodiscard]] bool isDataArtifact(const ProjectAsset& asset) {
    return asset.mime == "application/x-data-artifact" ||
           asset.mime == "application/vnd.apache.parquet" || asset.mime == "application/avro" ||
           asset.mime == "application/x-numpy" || asset.mime == "application/x-hdf5" ||
           asset.mime == "application/x-sqlite3" || asset.extension == ".csv" ||
           asset.extension == ".tsv" || asset.extension == ".jsonl" || asset.extension == ".ndjson";
}

[[nodiscard]] bool isBinaryArtifactMime(const std::string& mime) {
    return mime == "application/octet-stream" || mime == "application/wasm" ||
           mime == "application/x-executable" || mime == "application/java-vm" ||
           mime == "application/vnd.android.dex" || mime == "application/x-mach-binary" ||
           mime == "application/x-plist";
}

} // namespace

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
    if (ArchiveExtractor::isArchivePath(asset.relativePath) || isArchiveMime(asset.mime)) {
        return AssetRole::Archive;
    }
    if (name == "cmakelists.txt" || name == "makefile" || name == "dockerfile" ||
        name == "package.json" || name == "pyproject.toml" || name == "pom.xml" ||
        name == "build.gradle" || name == "settings.gradle" || name == "vite.config.js" ||
        name == "webpack.config.js" || name == "tsconfig.json") {
        return AssetRole::BuildSystem;
    }
    if (name == "requirements.txt" || name == "package-lock.json" || name == "pnpm-lock.yaml" ||
        name == "yarn.lock" || name == "cargo.toml" || name == "cargo.lock" || name == "go.mod" ||
        name == "go.sum" || name == "poetry.lock" || name == "composer.json" || name == "gemfile" ||
        name == "gemfile.lock") {
        return AssetRole::DependencyManifest;
    }
    if (!asset.language.empty() || isCodeExtension(asset.extension)) {
        return AssetRole::SourceCode;
    }
    if (name == "readme.md" || name == "readme.txt" || name == "project.md" ||
        name == "overview.md" || util::contains(original, "项目介绍") ||
        util::contains(original, "项目说明") || util::contains(original, "申报") ||
        util::contains(path, "declaration") || util::contains(path, "proposal")) {
        return AssetRole::ProjectDeclaration;
    }
    if (util::contains(original, "商业计划") || util::contains(path, "business_plan")) {
        return AssetRole::BusinessPlan;
    }
    if (util::contains(original, "路演") || util::contains(path, "pitch") ||
        asset.extension == ".pptx" || asset.extension == ".pptm" || asset.extension == ".ppsx" ||
        asset.extension == ".odp") {
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
        isDataArtifact(asset)) {
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
    if (asset.mime.rfind("image/", 0) == 0U || asset.mime.rfind("video/", 0) == 0U ||
        asset.mime.rfind("audio/", 0) == 0U || asset.mime.rfind("font/", 0) == 0U) {
        return AssetRole::ResourceAsset;
    }
    if (asset.mime == "application/x-model-artifact" || asset.mime.starts_with("model/")) {
        return AssetRole::ModelArtifact;
    }
    if (isBinaryArtifactMime(asset.mime)) {
        return AssetRole::BinaryArtifact;
    }
    return AssetRole::Unknown;
}

} // namespace cc
