/**
 * @file StagedAuditEngine.hpp
 * @brief 可分步执行的核心审计引擎。
 *
 * StagedAuditEngine 把端到端审计拆成一串确定性步骤，让上层（会话工作区）能在每一步
 * 之间取回真实的中间结果并流式展示，而不必依赖 UI 层伪造进度。步骤序列是审计流程的
 * 唯一真相源，AuditEngine 只是它的一次性驱动器。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

#include <string>
#include <vector>

namespace cc {

/**
 * @brief 单个审计步骤的稳定标识。
 *
 * 名称与 ToolRegistry 中注册的审计工具 name 保持一致，使会话工具卡片可以直接对应。
 */
struct AuditStageInfo {
    std::string name;
    std::string title;
};

/**
 * @brief 单个审计步骤执行后的真实结果摘要。
 *
 * detail 由已经算好的中间数据生成，例如资产数量、声明条数、评分等，供会话流展示真实观察，
 * 不再使用固定文案。
 */
struct AuditStageOutcome {
    std::string name;
    std::string title;
    std::string detail;
    bool ok{true};
};

/**
 * @brief 可分步执行的审计引擎。
 *
 * 使用方式：构造后先设置 ProjectContext 与 AuditOptions，然后反复调用 advance() 逐步推进；
 * 每步返回该步的真实结果摘要。全部完成后调用 takeResult() 取回完整 AuditResult。
 * 该类不负责解包或创建工作区，输入应为 ProjectLoader 准备好的隔离上下文。
 */
class StagedAuditEngine {
  public:
    /** @brief 返回审计流程的步骤序列（唯一真相源）。 */
    [[nodiscard]] static const std::vector<AuditStageInfo>& stages();

    /** @brief 绑定已隔离的项目上下文和审计选项，重置内部状态。 */
    void reset(const ProjectContext& context, const AuditOptions& options);

    /** @brief 是否还有未执行的步骤。 */
    [[nodiscard]] bool hasNext() const;

    /** @brief 当前已完成的步骤数量。 */
    [[nodiscard]] std::size_t completedStages() const;

    /**
     * @brief 执行下一个步骤。
     *
     * @return 成功时返回该步骤真实结果摘要；失败时返回该步骤的错误原因（如规则校验失败）。
     */
    [[nodiscard]] Result<AuditStageOutcome> advance();

    /** @brief 取回累积的完整审计结果（应在所有步骤完成后调用）。 */
    [[nodiscard]] AuditResult takeResult();

  private:
    ProjectContext context_;
    AuditOptions options_;
    AuditResult result_;
    CompetitionTypeResult typeResult_;
    std::size_t stageIndex_{0};
    bool started_{false};
};

} // namespace cc
