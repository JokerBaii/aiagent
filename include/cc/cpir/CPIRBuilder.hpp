/**
 * @file CPIRBuilder.hpp
 * @brief CPIR 项目中间表示构建。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

/**
 * @brief CPIR 项目中间表示构建器。
 *
 * 本类从已抽取文本中提取结构化字段；材料不足时只标记缺失，不生成虚假内容。
 */
class CPIRBuilder {
  public:
    /**
     * @brief 使用详细赛道识别结果构建 CPIR。
     */
    [[nodiscard]] CPIR build(const ProjectInventory& inventory,
                             const std::vector<TextDocument>& corpus,
                             const CompetitionTypeResult& type) const;
    /**
     * @brief 使用赛道枚举构建 CPIR 的兼容接口。
     */
    [[nodiscard]] CPIR build(const ProjectInventory& inventory,
                             const std::vector<TextDocument>& corpus, CompetitionType type) const;
};

} // namespace cc
