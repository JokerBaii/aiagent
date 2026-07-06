/**
 * @file InventoryEngine.cpp
 * @brief 项目资产语义清单构建实现。
 */

#include "cc/inventory/InventoryEngine.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/inventory/GeneratedVendoredDetector.hpp"
#include "cc/inventory/RoleClassifier.hpp"
#include "cc/inventory/SensitiveFileDetector.hpp"

namespace cc {

bool hasRole(const ProjectInventory& inventory, AssetRole role) {
    const auto iter = inventory.roleCounts.find(role);
    return iter != inventory.roleCounts.end() && iter->second > 0U;
}

std::size_t countRole(const ProjectInventory& inventory, AssetRole role) {
    const auto iter = inventory.roleCounts.find(role);
    return iter == inventory.roleCounts.end() ? 0U : iter->second;
}

std::vector<std::filesystem::path> filesWithRole(const ProjectInventory& inventory,
                                                 AssetRole role) {
    std::vector<std::filesystem::path> files;
    for (const auto& asset : inventory.assets) {
        if (asset.role == role) {
            files.push_back(asset.relativePath);
        }
    }
    return files;
}

Result<ProjectInventory> InventoryEngine::build(const ProjectContext& context) const {
    const auto scanRoot = context.inputRoot.empty() ? context.originalRoot : context.inputRoot;
    std::error_code ec;
    if (!std::filesystem::exists(scanRoot, ec) || !std::filesystem::is_directory(scanRoot, ec)) {
        return Result<ProjectInventory>::failure("项目根目录不存在或不可读");
    }

    ProjectInventory inventory;
    inventory.root = scanRoot;
    FormatDetector formatDetector;
    SensitiveFileDetector sensitiveDetector;
    GeneratedVendoredDetector generatedDetector;
    RoleClassifier roleClassifier;

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iter(scanRoot, options, ec);
         !ec && iter != std::filesystem::recursive_directory_iterator(); iter.increment(ec)) {
        const auto path = iter->path();
        if (iter->is_directory(ec)) {
            const auto name = path.filename().string();
            if (name == ".git" || name == ".workspaces") {
                iter.disable_recursion_pending();
            }
            continue;
        }
        if (!iter->is_regular_file(ec)) {
            continue;
        }

        auto asset = formatDetector.detect(scanRoot, path);
        asset.generated = generatedDetector.isGenerated(asset.relativePath);
        asset.vendored = generatedDetector.isVendored(asset.relativePath);
        asset.sensitive = sensitiveDetector.isSensitive(path);
        asset.role = roleClassifier.classify(asset);
        asset.auditable =
            asset.auditable && !asset.sensitive && !asset.generated && !asset.vendored;
        if (asset.sensitive) {
            asset.riskFlags.push_back("SECRET_RISK");
        }
        if (asset.generated) {
            asset.riskFlags.push_back("GENERATED");
        }
        if (asset.vendored) {
            asset.riskFlags.push_back("VENDORED");
        }
        if (asset.role == AssetRole::Archive) {
            asset.riskFlags.push_back("NESTED_ARCHIVE_NEEDS_REVIEW");
        }
        asset.importance = asset.role == AssetRole::Unknown ? 1 : 3;
        ++inventory.roleCounts[asset.role];
        inventory.assets.push_back(std::move(asset));
    }

    if (ec) {
        inventory.warnings.push_back("扫描部分目录失败: " + ec.message());
    }
    if (!hasRole(inventory, AssetRole::ProjectDeclaration)) {
        inventory.warnings.push_back("未发现项目申报材料。");
    }
    if (hasRole(inventory, AssetRole::SecretRisk)) {
        inventory.warnings.push_back("发现敏感文件风险。");
    }
    return Result<ProjectInventory>::success(inventory);
}

} // namespace cc
