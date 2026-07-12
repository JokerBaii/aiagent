#include "../TestSupport.hpp"

#include "cc/inventory/FormatDetector.hpp"
#include "cc/inventory/GeneratedVendoredDetector.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/inventory/RoleClassifier.hpp"
#include "cc/inventory/SensitiveFileDetector.hpp"
#include "cc/loader/ProjectLoader.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>

namespace {

[[nodiscard]] const cc::ProjectAsset& assetAt(const cc::ProjectInventory& inventory,
                                              std::string_view path) {
    const auto found =
        std::find_if(inventory.assets.begin(), inventory.assets.end(), [&](const auto& asset) {
            return asset.relativePath.generic_string() == path;
        });
    if (found == inventory.assets.end()) {
        throw std::runtime_error("asset missing: " + std::string{path});
    }
    return *found;
}

} // namespace

void runInventoryTests() {
    struct MetadataCase {
        std::filesystem::path path;
        std::string_view format;
        std::string_view mimePrefix;
        cc::AssetRole role;
    };
    const std::array metadataCases{
        MetadataCase{"src/main.rs", "rs", "text/x-source-code", cc::AssetRole::SourceCode},
        MetadataCase{"材料/申报书.docx", "docx", "application/vnd.openxmlformats",
                     cc::AssetRole::ProjectDeclaration},
        MetadataCase{"材料/旧版申报书.doc", "doc", "application/msword",
                     cc::AssetRole::ProjectDeclaration},
        MetadataCase{"材料/证明.pdf", "pdf", "application/pdf", cc::AssetRole::ProofMaterial},
        MetadataCase{"assets/photo.avif", "avif", "image/", cc::AssetRole::ResourceAsset},
        MetadataCase{"demo/video.mkv", "mkv", "video/", cc::AssetRole::ResourceAsset},
        MetadataCase{"interview.opus", "opus", "audio/", cc::AssetRole::ResourceAsset},
        MetadataCase{"models/network.onnx", "onnx", "application/x-model-artifact",
                     cc::AssetRole::ModelArtifact},
        MetadataCase{"models/scene.glb", "glb", "model/", cc::AssetRole::ModelArtifact},
        MetadataCase{"submission.rar", "archive", "application/archive", cc::AssetRole::Archive},
        MetadataCase{"data/cache.sqlite", "sqlite", "application/octet-stream",
                     cc::AssetRole::BinaryArtifact},
        MetadataCase{"assets/vendor.opaque", "opaque", "application/octet-stream",
                     cc::AssetRole::BinaryArtifact},
    };
    const cc::FormatDetector formatDetector;
    const cc::RoleClassifier roleClassifier;
    for (const auto& testCase : metadataCases) {
        auto asset = formatDetector.detectMetadata(testCase.path, 42U);
        asset.role = roleClassifier.classify(asset);
        requireTrue(asset.format == testCase.format,
                    "metadata format mismatch for " + testCase.path.generic_string());
        requireTrue(asset.mime.starts_with(testCase.mimePrefix),
                    "metadata MIME mismatch for " + testCase.path.generic_string());
        requireTrue(asset.role == testCase.role,
                    "metadata role mismatch for " + testCase.path.generic_string());
        requireTrue(!asset.auditable,
                    "metadata-only assets must never become auditable without content");
    }

    const auto magicRoot = std::filesystem::temp_directory_path() / "contest_inventory_magic";
    std::filesystem::remove_all(magicRoot);
    std::filesystem::create_directories(magicRoot);
    {
        std::ofstream(magicRoot / "document.unknown", std::ios::binary) << "%PDF-1.7\n";
        std::ofstream archive(magicRoot / "package.unknown", std::ios::binary);
        const std::array<unsigned char, 4> zipHeader{'P', 'K', 0x03U, 0x04U};
        archive.write(reinterpret_cast<const char*>(zipHeader.data()),
                      static_cast<std::streamsize>(zipHeader.size()));
        std::ofstream model(magicRoot / "scene.unknown", std::ios::binary);
        const std::array<unsigned char, 12> glbHeader{'g', 'l', 'T', 'F', 2U, 0U,
                                                      0U,  0U,  12U, 0U,  0U, 0U};
        model.write(reinterpret_cast<const char*>(glbHeader.data()),
                    static_cast<std::streamsize>(glbHeader.size()));
    }
    auto magicPdf = formatDetector.detect(magicRoot, magicRoot / "document.unknown");
    requireTrue(magicPdf.format == "pdf" && magicPdf.mime == "application/pdf" &&
                    magicPdf.auditable,
                "PDF magic should be recognized without relying on the extension");
    auto magicArchive = formatDetector.detect(magicRoot, magicRoot / "package.unknown");
    requireTrue(magicArchive.format == "zip" &&
                    roleClassifier.classify(magicArchive) == cc::AssetRole::Archive,
                "archive magic should remain recognizable under an unknown extension");
    auto magicModel = formatDetector.detect(magicRoot, magicRoot / "scene.unknown");
    requireTrue(magicModel.format == "glb" &&
                    roleClassifier.classify(magicModel) == cc::AssetRole::ModelArtifact,
                "3D model magic should remain recognizable under an unknown extension");
    std::filesystem::remove_all(magicRoot);

    auto context = cc::ProjectLoader{}.load(sourceDir() / "examples/software_bad_case");
    requireTrue(context.ok(), "software example should load");
    auto inventory = cc::InventoryEngine{}.build(context.value());
    requireTrue(inventory.ok(), "inventory should build");
    requireTrue(cc::hasRole(inventory.value(), cc::AssetRole::SourceCode),
                "source code role missing");

    const auto root = std::filesystem::temp_directory_path() / "contest_inventory_negative";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "node_modules/pkg");
    std::filesystem::create_directories(root / "build/cache");
    {
        std::ofstream(root / "README.md") << "# 可信项目\n";
        std::ofstream binary(root / "disguised.md", std::ios::binary);
        const char bytes[] = {'M', 'Z', '\0', '\1'};
        binary.write(bytes, static_cast<std::streamsize>(sizeof(bytes)));
        std::ofstream noExtension(root / "blob", std::ios::binary);
        noExtension.write(bytes, static_cast<std::streamsize>(sizeof(bytes)));
        std::ofstream(root / "facts.custom") << "target_user: students\n";
        std::ofstream(root / ".env.production") << "API_KEY=real-secret-value\n";
        std::ofstream(root / "node_modules/pkg/index.js") << "secret source";
        std::ofstream(root / "build/cache/result.txt") << "generated";
    }

    cc::ProjectContext direct;
    direct.inputRoot = root;
    const auto negativeInventory = cc::InventoryEngine{}.build(direct);
    requireTrue(negativeInventory.ok(), "negative inventory fixture should build");
    const auto& disguised = assetAt(negativeInventory.value(), "disguised.md");
    requireTrue(!disguised.auditable && disguised.format == "pe" &&
                    disguised.mime == "application/x-executable",
                "executable content with a text extension must be identified and rejected");
    requireTrue(std::find(disguised.riskFlags.begin(), disguised.riskFlags.end(),
                          "EXTENSION_CONTENT_MISMATCH") != disguised.riskFlags.end(),
                "extension/content mismatch should be explicit");
    requireTrue(!assetAt(negativeInventory.value(), "blob").auditable,
                "extensionless binary data must not be treated as text");
    requireTrue(assetAt(negativeInventory.value(), "facts.custom").auditable,
                "unknown extensions with valid UTF-8 text should remain readable");
    requireTrue(assetAt(negativeInventory.value(), ".env.production").sensitive,
                ".env variants must be treated as sensitive");
    requireTrue(negativeInventory.value().assets.size() == 5U,
                "generated and vendored directories should be pruned from inventory");

    const cc::GeneratedVendoredDetector generated;
    requireTrue(generated.isVendored("node_modules/pkg/index.js"),
                "root-level node_modules must be detected");
    requireTrue(generated.isGenerated("build/output.bin"),
                "root-level build directory must be detected");
    requireTrue(cc::SensitiveFileDetector{}.isSensitive(root / ".env.production"),
                "sensitive detector should recognize environment variants");
    std::filesystem::remove_all(root);
}
