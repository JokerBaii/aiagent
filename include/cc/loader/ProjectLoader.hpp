/**
 * @file ProjectLoader.hpp
 * @brief 项目目录加载与工作区创建。
 */

#pragma once

#include "cc/core/ProjectModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class ProjectLoader {
  public:
    /**
     * @brief 导入项目目录或 zip 并创建隔离工作区。
     *
     * @param projectPath 用户提供的项目目录或 zip 文件。
     * @return 成功时返回 ProjectContext；失败时返回路径不存在、格式不支持或复制错误。
     */
    [[nodiscard]] Result<ProjectContext> load(const std::filesystem::path& projectPath) const;
};

} // namespace cc
