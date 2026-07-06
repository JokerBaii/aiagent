/**
 * @file ArchiveExtractor.hpp
 * @brief 压缩包输入边界。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 压缩包导入请求。
 */
struct ArchiveImportRequest {
    std::filesystem::path archivePath;
    std::filesystem::path workspaceRoot;
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
