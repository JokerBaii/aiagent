/**
 * @file GeneratedVendoredDetector.hpp
 * @brief 生成物和第三方依赖识别。
 */

#pragma once

#include <filesystem>

namespace cc {

/**
 * @brief 生成物和第三方依赖识别器。
 *
 * 生成物和 vendored 依赖不能计入自主贡献，因此需要在 PASI 阶段提前标记。
 */
class GeneratedVendoredDetector {
  public:
    /**
     * @brief 判断路径是否属于构建产物或自动生成目录。
     */
    [[nodiscard]] bool isGenerated(const std::filesystem::path& path) const;
    /**
     * @brief 判断路径是否属于第三方依赖目录。
     */
    [[nodiscard]] bool isVendored(const std::filesystem::path& path) const;
};

} // namespace cc
