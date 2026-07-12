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
#include <utility>
#include <vector>

namespace workbench {

/**
 * @brief 会话流中的一条可展示消息。
 *
 * context 字段保留来源模块或审计会话，避免智能体回复脱离工具轨迹和证据链。
 * kind 用于让会话流按角色内联渲染不同样式（用户气泡、计划、工具卡片、智能体回复、系统、产物），
 * 取值：user / plan / tool / assistant / system / artifact。缺省时由 role 推断。
 */
struct SessionMessage {
    SessionMessage() = default;
    SessionMessage(QString roleValue, QString textValue, QString contextValue = {},
                   QString kindValue = {}, QString detailValue = {}, bool okValue = true,
                   QString targetValue = {})
        : role{std::move(roleValue)}, text{std::move(textValue)}, context{std::move(contextValue)},
          kind{std::move(kindValue)}, detail{std::move(detailValue)},
          target{std::move(targetValue)}, ok{okValue} {}

    QString role;
    QString text;
    QString context;
    QString kind;
    QString detail;
    QString target;
    bool ok{true};
};

[[nodiscard]] QVariantMap projectContext(const cc::AuditResult* result,
                                         const QString& normalizedProjectPath);
[[nodiscard]] QVariantList sessionHistory(const cc::AuditResult* result,
                                          const std::vector<SessionMessage>& conversation,
                                          const QString& normalizedProjectPath);
[[nodiscard]] QVariantList toolCards(const cc::AuditResult* result,
                                     const std::optional<cc::AuditDiff>& auditDiff,
                                     bool agentRunning, int activeStep, int completedSteps);
[[nodiscard]] QVariantList permissionCards(bool llmApproved, const QString& accessMode);
[[nodiscard]] QVariantList artifacts(const cc::AuditResult* result,
                                     const std::optional<cc::AuditDiff>& auditDiff,
                                     const QString& agentResult);
[[nodiscard]] QString agentSummary(const cc::AuditResult* result);

} // namespace workbench
