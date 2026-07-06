/**
 * @file RulePackValidator.cpp
 * @brief 规则包字段完整性校验实现。
 */

#include "cc/rules/RulePackValidator.hpp"

#include <set>

namespace cc {

Result<void> RulePackValidator::validate(const std::vector<AuditRule>& rules) const {
    if (rules.empty()) {
        return Result<void>::failure("规则包没有规则");
    }
    std::set<std::string> ids;
    for (const auto& rule : rules) {
        if (rule.ruleId.empty() || rule.name.empty() || rule.track.empty() || rule.target.empty() ||
            rule.description.empty() || rule.failReason.empty() || rule.fixTask.empty() ||
            !rule.condition.isObject()) {
            return Result<void>::failure("规则字段不完整: " + rule.ruleId);
        }
        if (!ids.insert(rule.ruleId).second) {
            return Result<void>::failure("规则 ID 重复: " + rule.ruleId);
        }
    }
    return Result<void>::success();
}

} // namespace cc
