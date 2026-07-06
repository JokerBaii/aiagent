/**
 * @file RulePackValidator.hpp
 * @brief 规则包字段完整性校验。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/Result.hpp"

namespace cc {

/**
 * @brief 规则包字段完整性校验器。
 *
 * 规则缺少 rule_id、中文说明或补证任务时必须阻断，防止不可解释规则进入审计。
 */
class RulePackValidator {
  public:
    /**
     * @brief 校验规则列表是否满足规则包规范。
     *
     * @return 成功时为空 Result；失败时返回第一个不可接受规则的原因。
     */
    [[nodiscard]] Result<void> validate(const std::vector<AuditRule>& rules) const;
};

} // namespace cc
