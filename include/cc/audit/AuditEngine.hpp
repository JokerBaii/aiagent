/**
 * @file AuditEngine.hpp
 * @brief 已加载项目上下文上的核心审计引擎。
 *
 * AuditEngine 只接收 ProjectContext，不负责解包或创建工作区；这样路径安全边界由
 * ProjectLoader/ArchiveExtractor 先完成，审计模块只处理已经隔离的输入。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

class AuditEngine {
  public:
    /**
     * @brief 在已隔离的项目上下文上执行核心审计。
     *
     * @param context ProjectLoader 创建的项目上下文，扫描入口应位于 workspace/input。
     * @param options 赛道与规则包目录。
     * @return 成功时返回完整 AuditResult；失败时返回加载、规则或抽取错误。
     */
    [[nodiscard]] Result<AuditResult> run(const ProjectContext& context,
                                          const AuditOptions& options) const;
};

} // namespace cc
