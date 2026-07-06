/**
 * @file TrustScoreCalculator.hpp
 * @brief 可信评分计算。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

class TrustScoreCalculator {
  public:
    /**
     * @brief 根据规则风险、证据和一致性问题计算可信评分。
     *
     * @param inventory 资产清单，用于材料完整性和生成物比例判断。
     * @param findings 规则引擎输出的风险项。
     * @param matches 声明证据匹配结果。
     * @param issues 材料一致性问题。
     * @return 确定性可信评分；本函数不调用 LLM。
     */
    [[nodiscard]] TrustScore calculate(const ProjectInventory& inventory,
                                       const std::vector<AuditFinding>& findings,
                                       const std::vector<EvidenceMatch>& matches,
                                       const std::vector<ConsistencyIssue>& issues) const;
};

} // namespace cc
