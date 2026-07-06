/**
 * @file TextExtractionService.hpp
 * @brief 可审计文本抽取服务。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class TextExtractionService {
  public:
    /**
     * @brief 从资产清单抽取可审计文本语料。
     *
     * @param inventory 已完成敏感文件、生成物和第三方依赖过滤的资产清单。
     * @return 成功时返回 TextDocument 列表；单个文件不可抽取时以状态标记或跳过。
     */
    [[nodiscard]] Result<std::vector<TextDocument>>
    extract(const ProjectInventory& inventory) const;
};

} // namespace cc
