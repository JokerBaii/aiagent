/**
 * @file InventoryEngine.cpp
 * @brief 项目资产语义清单构建实现。
 */

#include "cc/inventory/InventoryEngine.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/inventory/GeneratedVendoredDetector.hpp"
#include "cc/inventory/RoleClassifier.hpp"
#include "cc/inventory/SensitiveFileDetector.hpp"
#include "cc/loader/ImportLimits.hpp"
#include "cc/loader/PathGuard.hpp"

#include <algorithm>

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
    std::sort(files.begin(), files.end());
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
    const ImportLimits limits;
    std::size_t skippedDirectories = 0U;
    std::size_t formatMismatches = 0U;
    bool inventoryLimitReached = false;

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator iter(scanRoot, options, ec);
         !ec && iter != std::filesystem::recursive_directory_iterator(); iter.increment(ec)) {
        const auto path = iter->path();
        if (iter->is_directory(ec)) {
            const auto relative = std::filesystem::relative(path, scanRoot, ec);
            if (ec) {
                break;
            }
            const auto name = path.filename().string();
            if (name == ".git" || name == ".workspaces" ||
                generatedDetector.isGenerated(relative) || generatedDetector.isVendored(relative)) {
                iter.disable_recursion_pending();
                ++skippedDirectories;
            }
            continue;
        }
        if (iter->is_symlink(ec)) {
            continue;
        }
        if (!iter->is_regular_file(ec)) {
            continue;
        }
        if (inventory.assets.size() >= limits.maxFileCount) {
            inventoryLimitReached = true;
            break;
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
        if (std::find(asset.riskFlags.begin(), asset.riskFlags.end(),
                      "EXTENSION_CONTENT_MISMATCH") != asset.riskFlags.end()) {
            ++formatMismatches;
        }
        asset.importance = asset.role == AssetRole::Unknown ? 1 : 3;
        ++inventory.roleCounts[asset.role];
        inventory.assets.push_back(std::move(asset));
    }

    if (ec) {
        inventory.warnings.push_back("扫描部分目录失败: " + ec.message());
    }
    if (inventoryLimitReached) {
        inventory.warnings.push_back("资产清单达到 " + std::to_string(limits.maxFileCount) +
                                     " 个条目的有界预算；已保留可用的部分清单");
    }
    std::size_t deferredCount = 0U;
    ec.clear();
    const bool originalIsSingleFile =
        !context.archiveInput && std::filesystem::is_regular_file(context.originalRoot, ec);
    ec.clear();
    for (const auto& deferred : context.deferredFiles) {
        const auto alreadyPresent = std::any_of(inventory.assets.begin(), inventory.assets.end(),
                                                [&](const ProjectAsset& asset) {
                                                    return asset.relativePath.lexically_normal() ==
                                                           deferred.relativePath.lexically_normal();
                                                });
        if (alreadyPresent) {
            continue;
        }

        auto asset = formatDetector.detectMetadata(deferred.relativePath, deferred.sizeBytes);
        const bool virtualOnly =
            context.archiveInput || context.unpackStatus == "INPUT_METADATA_ONLY";
        if (virtualOnly) {
            // Archive entries and selected special files have no independently readable regular
            // source. Sampling originalRoot would either inspect the container or follow a link.
            const auto virtualPath =
                context.workspaceRoot / ".deferred-metadata" / deferred.relativePath;
            asset.sensitive = sensitiveDetector.isSensitive(virtualPath);
        } else {
            const auto source = originalIsSingleFile ? context.originalRoot
                                                     : context.originalRoot / deferred.relativePath;
            if ((!originalIsSingleFile && !PathGuard::isInsideRoot(context.originalRoot, source)) ||
                std::filesystem::is_symlink(source, ec)) {
                ec.clear();
                continue;
            }
            ec.clear();
            asset.absolutePath = source;
            asset.sensitive = sensitiveDetector.isSensitive(source);
        }
        asset.generated = generatedDetector.isGenerated(asset.relativePath);
        asset.vendored = generatedDetector.isVendored(asset.relativePath);
        asset.auditable = false;
        asset.role = roleClassifier.classify(asset);
        asset.importance = asset.role == AssetRole::Unknown ? 1 : 3;
        asset.riskFlags.push_back("CONTENT_DEFERRED");
        if (!deferred.reason.empty()) {
            asset.riskFlags.push_back(deferred.reason);
        }
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
        ++inventory.roleCounts[asset.role];
        inventory.assets.push_back(std::move(asset));
        ++deferredCount;
    }
    std::sort(inventory.assets.begin(), inventory.assets.end(),
              [](const ProjectAsset& left, const ProjectAsset& right) {
                  return left.relativePath.generic_string() < right.relativePath.generic_string();
              });
    if (skippedDirectories > 0U) {
        inventory.warnings.push_back("已跳过 " + std::to_string(skippedDirectories) +
                                     " 个生成物或第三方依赖目录。");
    }
    if (formatMismatches > 0U) {
        inventory.warnings.push_back("发现 " + std::to_string(formatMismatches) +
                                     " 个扩展名与文件内容不一致的材料，已停止自动抽取。");
    }
    if (deferredCount > 0U) {
        inventory.warnings.push_back(
            "已在文件树中保留 " + std::to_string(deferredCount) +
            " 个未复制的大型或受限文件；可查看元数据，但不会自动读取全部内容");
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
