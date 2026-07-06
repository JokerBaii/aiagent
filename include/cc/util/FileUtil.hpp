/**
 * @file FileUtil.hpp
 * @brief 文件读写工具。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace cc::util {

/**
 * @brief 将路径转换为报告友好的通用字符串。
 */
[[nodiscard]] std::string pathString(const std::filesystem::path& path);
/**
 * @brief 按大小上限读取文件。
 *
 * 限制读取量是为了避免超大材料拖垮审计进程。
 */
[[nodiscard]] std::string readFileLimited(const std::filesystem::path& path, std::size_t limit);
/**
 * @brief 写入文本文件并按需创建父目录。
 *
 * 调用方必须确保目标位于允许的工作区或导出目录。
 */
[[nodiscard]] Result<void> writeTextFile(const std::filesystem::path& path,
                                         const std::string& content);

} // namespace cc::util
