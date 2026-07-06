/**
 * @file SensitiveFileDetector.hpp
 * @brief 敏感文件识别。
 */

#pragma once

#include <filesystem>

namespace cc {

/**
 * @brief 敏感文件识别器。
 *
 * 该类只做本地文件名和轻量内容检查，用于阻止密钥材料进入外部工具或报告正文。
 */
class SensitiveFileDetector {
  public:
    /**
     * @brief 判断文件是否属于密钥、token 或凭据风险。
     *
     * @param path 待检查文件路径。
     * @return 命中敏感规则时返回 true。
     */
    [[nodiscard]] bool isSensitive(const std::filesystem::path& path) const;
};

} // namespace cc
