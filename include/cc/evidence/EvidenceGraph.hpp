/**
 * @file EvidenceGraph.hpp
 * @brief 声明与证据关系图摘要。
 *
 * EvidenceGraph 不重新判断证据，只把 EvidenceMatcher 的确定性结果组织成可查询结构，
 * 便于 Analyzer、报告和二次审计复用同一份证据状态。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

#include <optional>

namespace cc {

class EvidenceGraph {
  public:
    /**
     * @brief 用确定性 EvidenceMatch 构建证据图摘要。
     */
    explicit EvidenceGraph(std::vector<EvidenceMatch> matches);

    /**
     * @brief 返回全部声明证据匹配关系。
     */
    [[nodiscard]] const std::vector<EvidenceMatch>& matches() const;
    /**
     * @brief 按声明 ID 查找证据状态。
     */
    [[nodiscard]] std::optional<EvidenceMatch> findByClaimId(const std::string& claimId) const;
    /**
     * @brief 计算 Supported + Partial/2 的证据覆盖率。
     */
    [[nodiscard]] double coveragePercent() const;

  private:
    std::vector<EvidenceMatch> matches_;
};

} // namespace cc
