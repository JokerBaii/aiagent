/**
 * @file RuleConditionEvaluator.hpp
 * @brief 单条规则条件的确定性评估。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 规则条件评估结果。
 *
 * RuleEngine 只负责规则遍历和 Finding 组装；条件命中、缺证列表和文件证据由
 * RuleConditionEvaluator 单独给出，避免规则引擎演变成大杂烩。
 */
struct RuleConditionResult {
    bool failed{false};
    std::vector<std::string> missing;
    std::vector<std::filesystem::path> evidence;
};

/**
 * @brief 根据 CPIR、资产、声明和证据评估一条规则的 condition。
 *
 * 本类只做可复核的条件判断，不生成最终审计结论，也不计算可信评分。
 */
class RuleConditionEvaluator {
  public:
    /**
     * @brief 评估规则条件是否触发。
     * @param rule JSON 规则包中的单条规则。
     * @param inventory 项目资产清单。
     * @param cpir 项目中间表示。
     * @param claims 已抽取声明。
     * @param matches 声明和证据的匹配结果。
     * @param issues 一致性审计发现的问题。
     * @return 规则条件触发状态、缺失证据说明和关联文件证据。
     */
    [[nodiscard]] RuleConditionResult evaluate(const AuditRule& rule,
                                               const ProjectInventory& inventory, const CPIR& cpir,
                                               const std::vector<ProjectClaim>& claims,
                                               const std::vector<EvidenceMatch>& matches,
                                               const std::vector<ConsistencyIssue>& issues) const;
};

} // namespace cc
