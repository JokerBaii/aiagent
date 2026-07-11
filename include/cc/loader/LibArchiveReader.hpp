/**
 * @file LibArchiveReader.hpp
 * @brief libarchive 压缩包安全读取。
 */

#pragma once

#include "cc/core/Result.hpp"
#include "cc/loader/ArchiveExtractionOutcome.hpp"
#include "cc/loader/ImportLimits.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace cc {

/**
 * @brief libarchive 条目摘要。
 */
struct LibArchiveEntry {
    std::filesystem::path relativePath;
    std::uint64_t sizeBytes{0};
    bool directory{false};
    bool symlink{false};
};

/**
 * @brief libarchive 解包请求。
 */
struct LibArchiveExtractionRequest {
    std::filesystem::path archivePath;
    std::filesystem::path destinationRoot;
    ImportLimits limits{};
};

/**
 * @brief 读取 tar/tgz/gz/7z 等 libarchive 支持的压缩包。
 *
 * 本类只负责枚举和解包普通文件，路径边界、符号链接和嵌套压缩包都会在写入前阻断，
 * 避免非 zip 压缩包绕过 ProjectLoader 的工作区安全策略。
 */
class LibArchiveReader {
  public:
    /**
     * @brief 列出压缩包条目。
     *
     * @param archivePath 压缩包路径。
     * @return 条目摘要；失败时返回 libarchive 错误。
     */
    [[nodiscard]] Result<std::vector<LibArchiveEntry>>
    list(const std::filesystem::path& archivePath, const ImportLimits& limits = {}) const;

    /**
     * @brief 解包到隔离工作区。
     *
     * @param request 解包请求。
     * @return 写入的相对文件路径；失败时返回路径、条目类型或解包错误。
     */
    [[nodiscard]] Result<ArchiveExtractionOutcome>
    extractAll(const LibArchiveExtractionRequest& request) const;
};

} // namespace cc
