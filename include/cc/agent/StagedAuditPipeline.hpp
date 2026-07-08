/**
 * @file StagedAuditPipeline.hpp
 * @brief 可分步执行、可流式观察的受控审计流水线。
 *
 * StagedAuditPipeline 在 StagedAuditEngine 之上补齐 agentic runtime 的编排职责：项目加载、
 * 生命周期 Hook 以及把每个步骤结果封装成会话可展示的 AgentObservation。会话工作区据此把
 * 真实的中间观察流式呈现给用户，取代 UI 层伪造的进度动画。
 */

#pragma once

#include "cc/agent/AgentModels.hpp"
#include "cc/audit/StagedAuditEngine.hpp"
#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 分步审计流水线。
 *
 * 生命周期：begin() 完成路径加载、BeforeProjectLoad Hook 与引擎绑定；随后反复调用
 * advance() 逐步执行核心审计并取回真实观察；hasNext() 为 false 后调用 finish() 触发
 * AfterAudit Hook 并取回完整结果。任何一步失败都会阻断后续步骤。
 */
class StagedAuditPipeline {
  public:
    /** @brief 返回审计步骤序列，便于 UI 预先渲染步骤骨架。 */
    [[nodiscard]] static const std::vector<AuditStageInfo>& stages();

    /** @brief 加载项目、执行前置 Hook 并绑定审计引擎。 */
    [[nodiscard]] Result<ProjectContext> begin(const std::filesystem::path& projectPath,
                                               const AuditOptions& options);

    /** @brief 是否仍有未执行步骤。 */
    [[nodiscard]] bool hasNext() const;

    /** @brief 已完成步骤数量。 */
    [[nodiscard]] std::size_t completedStages() const;

    /**
     * @brief 执行下一个审计步骤并返回真实观察。
     *
     * observation.output 中包含 stage 名称与真实 detail，会话流据此渲染工具卡片。
     */
    [[nodiscard]] Result<AgentObservation> advance();

    /**
     * @brief 执行 AfterAudit Hook 并取回完整审计结果。
     *
     * 必须在所有步骤完成后调用。
     */
    [[nodiscard]] Result<AuditResult> finish();

  private:
    StagedAuditEngine engine_;
    bool begun_{false};
};

} // namespace cc
