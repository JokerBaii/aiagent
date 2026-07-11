/**
 * @file ZipArchiveReader.hpp
 * @brief zip 压缩包目录读取与安全解压。
 *
 * 本模块只处理 zip 文件格式本身，不负责创建 ProjectContext。
 * ArchiveExtractor 会在此基础上执行竞赛项目输入边界校验。
 */

#pragma once

#include "cc/core/Result.hpp"
#include "cc/loader/ArchiveExtractionOutcome.hpp"
#include "cc/loader/ImportLimits.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief zip 中的单个条目元数据。
 *
 * 元数据来自中央目录。调用方必须先检查路径和符号链接，再允许写入工作区。
 */
struct ZipArchiveEntry {
    std::filesystem::path relativePath;
    bool directory{false};
    bool symlink{false};
    std::uint16_t compressionMethod{0};
    std::uint64_t compressedSize{0};
    std::uint64_t uncompressedSize{0};
};

/**
 * @brief zip 解压请求。
 *
 * 使用命名结构体承载两个路径，避免 archivePath 与 destinationRoot 在调用处被调换。
 */
struct ZipExtractionRequest {
    std::filesystem::path archivePath;
    std::filesystem::path destinationRoot;
    ImportLimits limits{};
};

/**
 * @brief zip 单条目文本读取请求。
 */
struct ZipEntryReadRequest {
    std::filesystem::path archivePath;
    std::filesystem::path entryPath;
    std::size_t maxBytes{0};
    ImportLimits limits{};
};

/**
 * @brief 读取和解压 zip 的最小安全实现。
 *
 * 当前支持 store 和 deflate 两种常见压缩方式，不支持加密 zip 和 Zip64。
 * 不调用 shell，也不执行压缩包中的任何内容。
 */
class ZipArchiveReader {
  public:
    /**
     * @brief 读取 zip 中央目录。
     *
     * @param archivePath zip 文件路径。
     * @return 成功时返回条目列表；失败时返回格式不支持或文件读取错误。
     */
    [[nodiscard]] Result<std::vector<ZipArchiveEntry>>
    list(const std::filesystem::path& archivePath, const ImportLimits& limits = {}) const;

    /**
     * @brief 将 zip 条目解压到目标目录。
     *
     * @param request zip 文件路径和工作区 input 目录。
     * @return 成功时返回解压出的普通文件相对路径；失败时返回格式或写入错误。
     */
    [[nodiscard]] Result<ArchiveExtractionOutcome>
    extractAll(const ZipExtractionRequest& request) const;

    /**
     * @brief 读取 zip 中单个文本条目。
     *
     * @param request zip 文件、条目路径和最大读取字节数。
     * @return 成功时返回条目文本；失败时返回条目缺失、格式不支持或大小超限。
     */
    [[nodiscard]] Result<std::string> readTextEntry(const ZipEntryReadRequest& request) const;
};

} // namespace cc
