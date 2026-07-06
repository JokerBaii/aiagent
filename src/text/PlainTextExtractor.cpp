/**
 * @file PlainTextExtractor.cpp
 * @brief 纯文本材料抽取实现。
 */

#include "cc/text/PlainTextExtractor.hpp"
#include "cc/util/FileUtil.hpp"

namespace cc {

Result<TextDocument> PlainTextExtractor::extract(const ProjectAsset& asset) const {
    constexpr std::size_t kMaxTextBytes = 512U * 1024U;
    TextDocument document;
    document.sourceFile = asset.relativePath;
    document.title = asset.fileName;
    document.text = util::readFileLimited(asset.absolutePath, kMaxTextBytes);
    document.status = document.text.empty() ? "EMPTY_OR_UNREADABLE" : "EXTRACTED";
    return Result<TextDocument>::success(std::move(document));
}

} // namespace cc
