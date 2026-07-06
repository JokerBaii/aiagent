/**
 * @file ProjectMemory.hpp
 * @brief 项目记忆文件初始化。
 */

#pragma once

#include "cc/core/Enums.hpp"
#include "cc/core/Result.hpp"

#include <filesystem>

namespace cc {

/**
 * @brief 项目记忆初始化器。
 *
 * Workbench 初始化流程会写入 .project-trust，用于记录赛道、权限、Hook 和不可无证据声明。
 */
class ProjectMemory {
  public:
    /**
     * @brief 初始化 .project-trust 项目记忆目录。
     *
     * @return 成功时写入 PROJECT_RULES.md、project_rules.json、permissions.json 和 hooks.json。
     */
    [[nodiscard]] Result<void> init(const std::filesystem::path& projectPath,
                                    CompetitionType track) const;
};

} // namespace cc
