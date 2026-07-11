/**
 * @file ArchiveExtractor.hpp
 * @brief 压缩包输入边界。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"
#include "cc/loader/ImportLimits.hpp"

#include <span>
#include <string>

namespace cc {

/**
 * @brief 压缩包导入请求。
 */
struct ArchiveImportRequest {
    std::filesystem::path archivePath;
    std::filesystem::path workspaceRoot;
    ImportLimits limits{};
};

enum class ArchiveFormat {
    Unknown,
    Zip,
    Gzip,
    SevenZip,
    Xz,
    Bzip2,
    Zstd,
    Rar,
    Tar,
    Cpio,
    Ar,
    Cab,
    Lz4,
};

enum class ArchiveReaderKind {
    None,
    NativeZip,
    LibArchive,
    MetadataOnly,
};

struct ArchiveProbe {
    bool archive{false};
    bool signatureMatched{false};
    ArchiveFormat format{ArchiveFormat::Unknown};
    ArchiveReaderKind reader{ArchiveReaderKind::None};
};

class ArchiveExtractor {
  public:
    /**
     * @brief 安全解包支持的压缩包到工作区 input 目录。
     *
     * @param request 压缩包路径和本次会话工作区根目录。
     * @return 成功时返回 ProjectContext；失败时返回路径穿越、嵌套压缩包或解包错误。
     */
    [[nodiscard]] Result<ProjectContext> extract(const ArchiveImportRequest& request) const;

    /**
     * @brief 在固定 512 字节预算内识别归档签名并结合已知扩展名给出安全路由。
     */
    [[nodiscard]] static Result<ArchiveProbe> probe(const std::filesystem::path& path);

    /**
     * @brief 只检查已在内存中的有界文件头，不使用路径扩展名。
     */
    [[nodiscard]] static ArchiveProbe probeHeader(std::span<const unsigned char> header);

    [[nodiscard]] static std::string formatName(ArchiveFormat format);
    /**
     * @brief 判断路径是否属于压缩包类型。
     *
     * 用于资产标记和嵌套压缩包阻断；不代表当前版本都能自动解包。
     */
    [[nodiscard]] static bool isArchivePath(const std::filesystem::path& path);
    /**
     * @brief 判断路径是否为当前版本可自动安全解包的压缩包。
     */
    [[nodiscard]] static bool isSupportedArchivePath(const std::filesystem::path& path);
};

} // namespace cc
