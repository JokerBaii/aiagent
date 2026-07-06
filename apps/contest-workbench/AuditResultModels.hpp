/**
 * @file AuditResultModels.hpp
 * @brief Workbench 展示模型映射。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace workbench {

[[nodiscard]] int blockerCount(const cc::AuditResult& result);
[[nodiscard]] int warningCount(const cc::AuditResult& result);
[[nodiscard]] QString summary(const cc::AuditResult& result);
[[nodiscard]] QVariantList assets(const cc::AuditResult& result);
[[nodiscard]] QVariantList roleDistribution(const cc::AuditResult& result);
[[nodiscard]] QVariantMap cpir(const cc::AuditResult& result);
[[nodiscard]] QVariantList claimEvidence(const cc::AuditResult& result);
[[nodiscard]] QVariantList consistencyIssues(const cc::AuditResult& result);
[[nodiscard]] QVariantList findings(const cc::AuditResult& result);
[[nodiscard]] QVariantList fixTasks(const cc::AuditResult& result);
[[nodiscard]] QVariantList scorePenalties(const cc::AuditResult& result);
[[nodiscard]] QVariantMap auditDiff(const cc::AuditDiff& diff);

} // namespace workbench
