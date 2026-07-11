/**
 * @file TextExtractionService.cpp
 * @brief 可审计文本抽取服务实现。
 */

#include "cc/text/TextExtractionService.hpp"
#include "cc/inventory/FormatDetector.hpp"
#include "cc/text/OpenXmlTextExtractor.hpp"
#include "cc/text/PdfTextExtractor.hpp"
#include "cc/text/PlainTextExtractor.hpp"
#include "cc/text/StructuredTextExtractor.hpp"

namespace cc {

Result<std::vector<TextDocument>>
TextExtractionService::extract(const ProjectInventory& inventory) const {
    std::vector<TextDocument> corpus;

    for (const auto& asset : inventory.assets) {
        if (!asset.auditable) {
            continue;
        }

        Result<TextDocument> document = Result<TextDocument>::failure("不支持的文本格式");
        if (isStructuredTextExtension(asset.extension)) {
            document = StructuredTextExtractor{}.extract(asset);
        } else if (isLikelyTextExtension(asset.extension) || isCodeExtension(asset.extension) ||
                   asset.extension.empty() || asset.mime.rfind("text/", 0U) == 0U) {
            document = PlainTextExtractor{}.extract(asset);
        } else if (isOfficeExtension(asset.extension)) {
            document = OpenXmlTextExtractor{}.extract(asset);
        } else if (asset.extension == ".pdf" || asset.mime == "application/pdf") {
            document = PdfTextExtractor{}.extract(asset);
        }
        if (!document.ok()) {
            TextDocument review;
            review.sourceFile = asset.relativePath;
            review.title = asset.fileName;
            review.status = "NEED_REVIEW_EXTRACTION_FAILED";
            corpus.push_back(std::move(review));
            continue;
        }
        corpus.push_back(std::move(document.value()));
    }

    return Result<std::vector<TextDocument>>::success(corpus);
}

} // namespace cc
