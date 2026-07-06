/**
 * @file CompetitionTypeDetector.hpp
 * @brief 竞赛类型识别。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

/**
 * @brief 竞赛类型识别器。
 *
 * 用户显式指定赛道时优先尊重用户输入；未指定时只根据材料特征做保守判断。
 */
class CompetitionTypeDetector {
  public:
    /**
     * @brief 返回带置信度和理由的赛道识别结果。
     *
     * @param inventory 资产清单。
     * @param corpus 已抽取文本。
     * @param requested 用户显式指定的赛道。
     */
    [[nodiscard]] CompetitionTypeResult detectDetailed(const ProjectInventory& inventory,
                                                       const std::vector<TextDocument>& corpus,
                                                       CompetitionType requested) const;
    /**
     * @brief 兼容旧调用方的简化赛道识别接口。
     */
    [[nodiscard]] CompetitionType detect(const ProjectInventory& inventory,
                                         const std::vector<TextDocument>& corpus,
                                         CompetitionType requested) const;
};

} // namespace cc
