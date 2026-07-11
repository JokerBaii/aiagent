/**
 * @file FormatDetector.hpp
 * @brief 文件格式和基础元数据检测。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"

namespace cc {

/**
 * @brief 文件格式和基础元数据检测器。
 *
 * 本类只识别格式、MIME、语言和可审计性，不判断材料角色或风险。
 */
class FormatDetector {
  public:
    /**
     * @brief 检测单个文件的基础资产信息。
     *
     * @param root 项目扫描根目录。
     * @param file 待检测文件路径。
     * @return ProjectAsset 基础字段，失败字段以保守默认值记录。
     */
    [[nodiscard]] ProjectAsset detect(const std::filesystem::path& root,
                                      const std::filesystem::path& file) const;

    /**
     * @brief 只根据相对路径和声明大小构建资产元数据。
     *
     * 该入口不会打开文件，适用于压缩包内未解压条目和因导入限制而延迟读取的文件。
     * 返回的资产始终不可审计，后续按需取得真实内容后才允许重新检测。
     */
    [[nodiscard]] ProjectAsset detectMetadata(const std::filesystem::path& relativePath,
                                              std::uintmax_t sizeBytes) const;
};

/**
 * @brief 判断扩展名是否适合按纯文本读取。
 */
[[nodiscard]] bool isLikelyTextExtension(const std::string& extension);
/**
 * @brief 判断扩展名是否属于常见源码或脚本文件。
 */
[[nodiscard]] bool isCodeExtension(const std::string& extension);
/**
 * @brief 判断扩展名是否属于 Office OpenXML 文档。
 */
[[nodiscard]] bool isOfficeExtension(const std::string& extension);

} // namespace cc
