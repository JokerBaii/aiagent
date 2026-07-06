/**
 * @file RulePackLoader.hpp
 * @brief JSON 规则包加载。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief JSON 规则包加载器。
 *
 * 加载器只负责读取和解析规则文件，字段完整性由 RulePackValidator 单独校验。
 */
class RulePackLoader {
  public:
    /**
     * @brief 加载 common 规则和指定赛道规则。
     */
    [[nodiscard]] Result<std::vector<AuditRule>>
    loadDirectory(const std::filesystem::path& rulesDir, CompetitionType track) const;
    /**
     * @brief 加载单个规则 JSON 文件。
     */
    [[nodiscard]] Result<std::vector<AuditRule>> loadFile(const std::filesystem::path& file) const;
};

} // namespace cc
