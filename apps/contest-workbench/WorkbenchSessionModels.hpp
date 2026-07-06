/**
 * @file WorkbenchSessionModels.hpp
 * @brief Workbench 会话工作区展示模型。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <vector>

namespace workbench {

/**
 * @brief 会话流中的一条可展示消息。
 *
 * context 字段保留来源模块或审计会话，避免顾问式回复脱离规则和证据链。
 */
struct SessionMessage {
    QString role;
    QString text;
    QString context;
};

[[nodiscard]] QVariantMap projectContext(const std::optional<cc::AuditResult>& result,
                                         const QString& normalizedProjectPath);
[[nodiscard]] QVariantList sessionHistory(const std::optional<cc::AuditResult>& result,
                                          const std::vector<SessionMessage>& conversation,
                                          const QString& normalizedProjectPath);
[[nodiscard]] QVariantList toolCards(const std::optional<cc::AuditResult>& result,
                                     const std::optional<cc::AuditDiff>& auditDiff,
                                     bool agentRunning, int activeStep,
                                     int completedSteps);
[[nodiscard]] QVariantList permissionCards(bool llmApproved);
[[nodiscard]] QVariantList artifacts(const std::optional<cc::AuditResult>& result,
                                     const std::optional<cc::AuditDiff>& auditDiff,
                                     const QString& llmAdvice);
[[nodiscard]] QString advisorSummary(const std::optional<cc::AuditResult>& result);

} // namespace workbench
