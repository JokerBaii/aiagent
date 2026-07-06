/**
 * @file LifecycleHookManager.cpp
 * @brief 生命周期 Hook 调度实现。
 */

#include "cc/agent/LifecycleHookManager.hpp"

namespace cc {

std::vector<std::string> LifecycleHookManager::builtinHooks() const {
    return {"PathSafetyHook",         "SensitiveFileHook",      "NoOriginalOverwriteHook",
            "RulePackValidationHook", "ReportCompletenessHook", "NoFabricatedEvidenceHook"};
}

Result<void> LifecycleHookManager::run(HookPoint point, const AuditResult* result) const {
    if (point == HookPoint::AfterAudit && result != nullptr) {
        for (const auto& finding : result->findings) {
            if (finding.ruleId.empty()) {
                return Result<void>::failure("RulePackValidationHook: finding 缺少 rule_id");
            }
        }
        if (result->trustScore.totalScore < 0 || result->trustScore.totalScore > 100) {
            return Result<void>::failure("ReportCompletenessHook: 可信评分越界");
        }
    }
    return Result<void>::success();
}

} // namespace cc
