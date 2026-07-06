/**
 * @file PathGuard.hpp
 * @brief 路径安全边界检查。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <filesystem>
#include <string>

namespace cc {

class PathGuard {
  public:
    /**
     * @brief 将路径规范化为可比较形式。
     *
     * @param path 待规范化路径。
     * @return 成功时返回规范化路径；失败时返回文件系统错误。
     */
    [[nodiscard]] static Result<std::filesystem::path> normalize(const std::filesystem::path& path);
    /**
     * @brief 检查目标路径是否仍位于 root 内。
     *
     * 该检查用于阻止解包和复制流程写出工作区边界。
     */
    [[nodiscard]] static bool isInsideRoot(const std::filesystem::path& root,
                                           const std::filesystem::path& target);
    /**
     * @brief 检查压缩包条目名是否安全。
     *
     * 拒绝绝对路径、反斜杠和 ..，防止 zip-slip 路径穿越。
     */
    [[nodiscard]] static bool isSafeArchiveEntry(const std::string& entryName);
};

} // namespace cc
