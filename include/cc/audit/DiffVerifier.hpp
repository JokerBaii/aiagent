/**
 * @file DiffVerifier.hpp
 * @brief 二次审计差分。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class DiffVerifier {
  public:
    /**
     * @brief 比较两份 JSON 审计包。
     *
     * @param oldAudit 修复前 audit.json。
     * @param newAudit 修复后 audit.json。
     * @return 成功时返回 AuditDiff；失败时返回文件读取或 JSON 解析错误。
     */
    [[nodiscard]] Result<AuditDiff> diffFiles(const std::filesystem::path& oldAudit,
                                              const std::filesystem::path& newAudit) const;
};

} // namespace cc
