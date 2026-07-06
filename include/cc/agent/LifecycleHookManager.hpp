/**
 * @file LifecycleHookManager.hpp
 * @brief 生命周期 Hook 调度。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 生命周期 Hook 管理器。
 *
 * Hook 用于在关键节点阻断不可解释报告、评分越界或缺少 rule_id 的审计结果。
 */
class LifecycleHookManager {
  public:
    /** @brief 返回内置 Hook 名称。 */
    [[nodiscard]] std::vector<std::string> builtinHooks() const;
    /**
     * @brief 执行指定生命周期 Hook。
     *
     * @return 成功时允许流程继续；失败时必须阻断当前审计或导出。
     */
    [[nodiscard]] Result<void> run(HookPoint point, const AuditResult* result = nullptr) const;
};

} // namespace cc
