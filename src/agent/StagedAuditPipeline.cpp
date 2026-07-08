/**
 * @file StagedAuditPipeline.cpp
 * @brief 可分步执行、可流式观察的受控审计流水线实现。
 */

#include "cc/agent/StagedAuditPipeline.hpp"

#include "cc/agent/LifecycleHookManager.hpp"
#include "cc/loader/ProjectLoader.hpp"

#include <utility>

namespace cc {

const std::vector<AuditStageInfo>& StagedAuditPipeline::stages() {
    return StagedAuditEngine::stages();
}

Result<ProjectContext> StagedAuditPipeline::begin(const std::filesystem::path& projectPath,
                                                  const AuditOptions& options) {
    LifecycleHookManager hooks;
    auto hook = hooks.run(HookPoint::BeforeProjectLoad);
    if (!hook.ok()) {
        return Result<ProjectContext>::failure(hook.error());
    }

    auto context = ProjectLoader{}.load(projectPath);
    if (!context.ok()) {
        return Result<ProjectContext>::failure(context.error());
    }

    engine_.reset(context.value(), options);
    begun_ = true;
    return Result<ProjectContext>::success(context.value());
}

bool StagedAuditPipeline::hasNext() const {
    return begun_ && engine_.hasNext();
}

std::size_t StagedAuditPipeline::completedStages() const {
    return engine_.completedStages();
}

Result<AgentObservation> StagedAuditPipeline::advance() {
    if (!begun_) {
        return Result<AgentObservation>::failure("审计流水线尚未开始");
    }
    auto stage = engine_.advance();
    if (!stage.ok()) {
        return Result<AgentObservation>::failure(stage.error());
    }
    const auto& outcome = stage.value();
    return Result<AgentObservation>::success(AgentObservation{
        .callId = outcome.name,
        .toolName = outcome.name,
        .ok = outcome.ok,
        .summary = outcome.title + "：" + outcome.detail,
        .output = JsonValue::Object{
            {"stage", outcome.name}, {"title", outcome.title}, {"detail", outcome.detail}}});
}

Result<AuditResult> StagedAuditPipeline::finish() {
    if (!begun_) {
        return Result<AuditResult>::failure("审计流水线尚未开始");
    }
    auto result = engine_.takeResult();
    LifecycleHookManager hooks;
    auto hook = hooks.run(HookPoint::AfterAudit, &result);
    if (!hook.ok()) {
        return Result<AuditResult>::failure(hook.error());
    }
    return Result<AuditResult>::success(std::move(result));
}

} // namespace cc
