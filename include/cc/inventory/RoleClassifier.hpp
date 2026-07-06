/**
 * @file RoleClassifier.hpp
 * @brief 项目资产语义角色分类。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

/**
 * @brief 项目资产语义角色分类器。
 *
 * 分类结果供规则引擎和报告使用，本类不读取文件正文，也不产生审计结论。
 */
class RoleClassifier {
  public:
    /**
     * @brief 根据文件名、路径、格式和风险标记判断资产角色。
     *
     * @param asset 已完成格式检测和敏感/生成物标记的资产。
     * @return 资产角色；无法判断时返回 AssetRole::Unknown。
     */
    [[nodiscard]] AssetRole classify(const ProjectAsset& asset) const;
};

} // namespace cc
