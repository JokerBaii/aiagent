/**
 * @file AuditPipeline.hpp
 * @brief agentic runtime 的端到端审计流程编排。
 *
 * Pipeline 只负责调用各模块和生命周期 Hook，不实现文件识别、规则判断、
 * 评分或报告拼接，避免形成万能 Manager。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class AuditPipeline {
  public:
    /**
     * @brief 从项目路径运行完整审计流水线。
     *
     * @param projectPath 用户提供的项目目录或 zip 文件。
     * @param options 赛道和规则包配置。
     * @return 成功时返回审计结果；失败时返回路径、解包、规则校验或 Hook 错误。
     */
    [[nodiscard]] Result<AuditResult> run(const std::filesystem::path& projectPath,
                                          const AuditOptions& options) const;
};

} // namespace cc
