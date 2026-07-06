/**
 * @file InventoryEngine.hpp
 * @brief 项目资产语义清单构建。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 项目资产语义清单构建器。
 *
 * InventoryEngine 只扫描 ProjectContext.inputRoot，确保审计基于隔离工作区而非原始项目。
 */
class InventoryEngine {
  public:
    /**
     * @brief 构建项目资产清单。
     *
     * @param context ProjectLoader 创建的安全上下文。
     * @return 成功时返回 ProjectInventory；失败时返回项目不可扫描原因。
     */
    [[nodiscard]] Result<ProjectInventory> build(const ProjectContext& context) const;
};

/**
 * @brief 判断清单中是否存在某类资产。
 */
[[nodiscard]] bool hasRole(const ProjectInventory& inventory, AssetRole role);
/**
 * @brief 统计某类资产数量。
 */
[[nodiscard]] std::size_t countRole(const ProjectInventory& inventory, AssetRole role);
/**
 * @brief 返回某类资产的相对路径列表。
 */
[[nodiscard]] std::vector<std::filesystem::path> filesWithRole(const ProjectInventory& inventory,
                                                               AssetRole role);

} // namespace cc
