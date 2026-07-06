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
};

/**
 * @brief 判断扩展名是否适合按纯文本读取。
 */
[[nodiscard]] bool isLikelyTextExtension(const std::string& extension);
/**
 * @brief 判断扩展名是否属于 Office OpenXML 文档。
 */
[[nodiscard]] bool isOfficeExtension(const std::string& extension);

} // namespace cc
