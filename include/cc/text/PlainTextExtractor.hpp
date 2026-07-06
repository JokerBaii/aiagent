/**
 * @file PlainTextExtractor.hpp
 * @brief 纯文本材料抽取。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class PlainTextExtractor {
  public:
    /**
     * @brief 读取 Markdown/TXT/源码等纯文本资产。
     *
     * @param asset 已被 PASI 标记为可审计文本的资产。
     * @return 成功时返回 TextDocument；读取失败时返回空文本并显式设置状态。
     */
    [[nodiscard]] Result<TextDocument> extract(const ProjectAsset& asset) const;
};

} // namespace cc
