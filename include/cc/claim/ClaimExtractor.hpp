/**
 * @file ClaimExtractor.hpp
 * @brief 项目承诺性声明抽取。
 */

#pragma once

#include "cc/core/AuditModels.hpp"

namespace cc {

/**
 * @brief 项目承诺性声明抽取器。
 *
 * 本类基于材料中的显式文本规则抽取声明，禁止凭空生成用户、营收、合作或实验结论。
 */
class ClaimExtractor {
  public:
    /**
     * @brief 从文本语料抽取项目声明。
     *
     * @param corpus TextExtractionService 输出的可审计文本。
     * @return 声明列表；没有命中规则时返回空列表。
     */
    [[nodiscard]] std::vector<ProjectClaim> extract(const std::vector<TextDocument>& corpus) const;
};

} // namespace cc
