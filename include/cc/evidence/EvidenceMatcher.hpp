/**
 * @file EvidenceMatcher.hpp
 * @brief 声明与证据材料匹配。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief 声明—证据匹配器。
 *
 * 本类只根据资产角色和声明类型判断证据状态，不使用 LLM 推断证据存在。
 */
class EvidenceMatcher {
  public:
    /**
     * @brief 为每条声明寻找对应证据材料。
     *
     * @param claims ClaimExtractor 抽取出的声明。
     * @param inventory 项目资产清单。
     * @return 每条声明的证据状态和缺失证据清单。
     */
    [[nodiscard]] std::vector<EvidenceMatch> match(const std::vector<ProjectClaim>& claims,
                                                   const ProjectInventory& inventory) const;
    [[nodiscard]] std::vector<EvidenceMatch> match(const std::vector<ProjectClaim>& claims,
                                                   const ProjectInventory& inventory,
                                                   const std::vector<TextDocument>& corpus) const;
};

/**
 * @brief 返回某类声明通常需要的证据材料。
 */
[[nodiscard]] std::vector<std::string> missingEvidenceForClaim(ClaimType type);

} // namespace cc
