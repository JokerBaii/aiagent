/**
 * @file AuditPipeline.cpp
 * @brief agentic runtime 的端到端审计流程编排实现。
 */

#include "cc/agent/AuditPipeline.hpp"
#include "cc/agent/LifecycleHookManager.hpp"
#include "cc/audit/AuditEngine.hpp"
#include "cc/loader/ProjectLoader.hpp"

namespace cc {

Result<AuditResult> AuditPipeline::run(const std::filesystem::path& projectPath,
                                       const AuditOptions& options) const {
    LifecycleHookManager hooks;
    auto hook = hooks.run(HookPoint::BeforeProjectLoad);
    if (!hook.ok()) {
        return Result<AuditResult>::failure(hook.error());
    }

    auto context = ProjectLoader{}.load(projectPath);
    if (!context.ok()) {
        return Result<AuditResult>::failure(context.error());
    }
    auto result = AuditEngine{}.run(context.value(), options);
    if (!result.ok()) {
        return Result<AuditResult>::failure(result.error());
    }

    hook = hooks.run(HookPoint::AfterAudit, &result.value());
    if (!hook.ok()) {
        return Result<AuditResult>::failure(hook.error());
    }
    return result;
}

} // namespace cc
