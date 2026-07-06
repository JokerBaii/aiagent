/**
 * @file OpenXmlTextExtractor.hpp
 * @brief docx/pptx/xlsx OpenXML 文本抽取。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class OpenXmlTextExtractor {
  public:
    /**
     * @brief 从 docx/pptx/xlsx 中抽取基础可审计文本。
     *
     * @param asset Office OpenXML 资产。
     * @return 成功时返回文本；无法抽取时返回 NEED_REVIEW 状态而不是编造内容。
     */
    [[nodiscard]] Result<TextDocument> extract(const ProjectAsset& asset) const;
};

} // namespace cc
