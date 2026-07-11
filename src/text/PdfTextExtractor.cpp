/**
 * @file PdfTextExtractor.cpp
 * @brief PDF 文本抽取实现。
 */

#include "cc/text/PdfTextExtractor.hpp"
#include "cc/text/PdfContentStreamParser.hpp"
#include "cc/util/FileUtil.hpp"

namespace cc {

Result<TextDocument> PdfTextExtractor::extract(const ProjectAsset& asset) const {
    constexpr std::size_t kMaxPdfBytes = 8U * 1024U * 1024U;
    TextDocument document;
    document.sourceFile = asset.relativePath;
    document.title = asset.fileName;

    // PDF 文本抽取不能绕过 ExecuteCommand 权限调用外部工具；这里只读取隔离工作区文件，
    // 并用保守解析器处理已有内容流，扫描件或复杂编码统一交给人工复核。
    const auto pdfBytes = util::readFileLimited(asset.absolutePath, kMaxPdfBytes);
    document.text = PdfContentStreamParser{}.extractText(pdfBytes);
    std::error_code error;
    const auto size = std::filesystem::file_size(asset.absolutePath, error);
    if (!error && size > kMaxPdfBytes) {
        document.status = "NEED_REVIEW_PDF_TRUNCATED";
    } else {
        document.status = document.text.empty() ? "NEED_REVIEW_PDF_TEXT_EXTRACTION_LIMITED"
                                                : "EXTRACTED_PDF";
    }
    return Result<TextDocument>::success(std::move(document));
}

} // namespace cc
