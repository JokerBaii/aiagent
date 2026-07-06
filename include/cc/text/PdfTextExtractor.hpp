/**
 * @file PdfTextExtractor.hpp
 * @brief PDF 文本抽取。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class PdfTextExtractor {
  public:
    /**
     * @brief 从可解析 PDF 中抽取文本。
     *
     * @param asset PDF 资产。
     * @return 成功时返回 PDF 文本；扫描件或解析失败时返回 NEED_REVIEW 状态。
     */
    [[nodiscard]] Result<TextDocument> extract(const ProjectAsset& asset) const;
};

} // namespace cc
