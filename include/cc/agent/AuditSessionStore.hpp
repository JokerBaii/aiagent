/**
 * @file AuditSessionStore.hpp
 * @brief 审计会话持久化。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 审计会话持久化器。
 *
 * 会话存储用于保存审计输入、工具输出和最终报告摘要，便于老师或团队复核。
 */
class AuditSessionStore {
  public:
    /** @brief 将 AuditResult 保存为 JSON 审计包。 */
    [[nodiscard]] Result<void> save(const AuditResult& result,
                                    const std::filesystem::path& output) const;
    /** @brief 将完整 AuditSession 保存为 JSON。 */
    [[nodiscard]] Result<void> save(const AuditSession& session,
                                    const std::filesystem::path& output) const;
};

} // namespace cc
