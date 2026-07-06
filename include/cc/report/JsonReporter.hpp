/**
 * @file JsonReporter.hpp
 * @brief JSON 审计包导出。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief JSON 审计包导出器。
 *
 * JSON 报告保留所有关键中间结果，供二次审计、Workbench 和外部复核工具使用。
 */
class JsonReporter {
  public:
    /**
     * @brief 将 AuditResult 转换为完整 JSON 对象。
     */
    [[nodiscard]] JsonValue toJson(const AuditResult& result) const;
    /**
     * @brief 将 JSON 审计包写入文件。
     */
    [[nodiscard]] Result<void> write(const AuditResult& result,
                                     const std::filesystem::path& output) const;
};

/** @brief 导出项目上下文。 */
[[nodiscard]] JsonValue contextToJson(const ProjectContext& context);
/** @brief 导出资产清单。 */
[[nodiscard]] JsonValue inventoryToJson(const ProjectInventory& inventory);
/** @brief 导出文本语料摘要。 */
[[nodiscard]] JsonValue corpusToJson(const std::vector<TextDocument>& corpus);
/** @brief 导出 CPIR 项目中间表示。 */
[[nodiscard]] JsonValue cpirToJson(const CPIR& cpir);
/** @brief 导出声明列表。 */
[[nodiscard]] JsonValue claimsToJson(const std::vector<ProjectClaim>& claims);
/** @brief 导出声明证据匹配结果。 */
[[nodiscard]] JsonValue evidenceToJson(const std::vector<EvidenceMatch>& matches);
/** @brief 导出一致性问题。 */
[[nodiscard]] JsonValue consistencyToJson(const std::vector<ConsistencyIssue>& issues);
/** @brief 导出规则风险项。 */
[[nodiscard]] JsonValue findingsToJson(const std::vector<AuditFinding>& findings);
/** @brief 导出补证任务。 */
[[nodiscard]] JsonValue fixTasksToJson(const std::vector<FixTask>& tasks);
/** @brief 导出可信评分。 */
[[nodiscard]] JsonValue trustScoreToJson(const TrustScore& score);
/** @brief 导出二次审计差分。 */
[[nodiscard]] JsonValue auditDiffToJson(const AuditDiff& diff);

} // namespace cc
