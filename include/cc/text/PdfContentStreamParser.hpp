/**
 * @file PdfContentStreamParser.hpp
 * @brief PDF 内容流文本解析。
 */

#pragma once

#include <string>
#include <string_view>

namespace cc {

/**
 * @brief 从 PDF 内容流中抽取显式文本。
 *
 * 解析器只处理 PDF 文件内已经存在的文本操作符，无法解析扫描图片时返回空文本，
 * 由上层标记为 NEED_REVIEW，避免把 OCR 或猜测结果混入可信审计。
 */
class PdfContentStreamParser {
  public:
    /**
     * @brief 抽取 PDF 内容流中的可见文本。
     *
     * @param pdfBytes PDF 文件字节。
     * @return 可解析文本；没有明确文本时返回空字符串。
     */
    [[nodiscard]] std::string extractText(std::string_view pdfBytes) const;
};

} // namespace cc
