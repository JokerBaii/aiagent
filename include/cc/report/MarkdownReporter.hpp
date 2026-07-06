/**
 * @file MarkdownReporter.hpp
 * @brief Markdown 可信审计报告导出。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief Markdown 可信审计报告导出器。
 *
 * Markdown 报告面向学生和老师阅读，必须保留规则 ID、证据来源和补证任务。
 */
class MarkdownReporter {
  public:
    /**
     * @brief 渲染中文 Markdown 审计报告。
     */
    [[nodiscard]] std::string render(const AuditResult& result) const;
    /**
     * @brief 将 Markdown 报告写入文件。
     */
    [[nodiscard]] Result<void> write(const AuditResult& result,
                                     const std::filesystem::path& output) const;
};

} // namespace cc
