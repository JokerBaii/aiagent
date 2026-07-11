#include "cc/rules/RulePackValidator.hpp"
#include "cc/core/Enums.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string_view>

namespace cc {
namespace {

constexpr std::size_t kMaxRules = 10000U;
constexpr std::size_t kMaxFieldLength = 8192U;

const std::set<std::string, std::less<>> kTracks{
    "common",          "business_innovation", "software_project",
    "engineering_product", "scientific_research", "social_practice",
    "public_welfare",  "ecommerce",           "ai_application",
    "comprehensive_innovation", "unknown"};

const std::set<std::string, std::less<>> kConditionKeys{
    "required_asset",              "required_assets",
    "required_any_assets",         "missing_asset",
    "missing_assets",              "minimum_asset_count",
    "required_claim_evidence",     "required_cpir_fields",
    "forbidden_roles",             "forbidden_sensitive_file",
    "consistency_check",           "doc_code_support",
    "business_model_completeness", "technical_route_completeness",
    "research_reproducibility",    "social_impact_evidence",
    "vendor_generated_ratio",      "max_ratio"};

const std::set<std::string, std::less<>> kBooleanConditions{
    "forbidden_sensitive_file",    "consistency_check",
    "doc_code_support",            "business_model_completeness",
    "technical_route_completeness", "research_reproducibility",
    "social_impact_evidence",      "vendor_generated_ratio"};

const std::set<std::string, std::less<>> kClaims{
    "usertraction", "marketscale", "technicalcapability", "businessmodel",
    "revenue",      "costreduction", "patent",              "copyright",
    "partnership",  "prototype",    "researchresult",      "socialimpact",
    "deployment"};

const std::set<std::string, std::less<>> kCpirFields{
    "project_name",         "target_user",          "pain_point",
    "solution",             "product_or_service",   "technical_route",
    "business_model",       "market_analysis",      "competitor_analysis",
    "financial_projection", "team_structure",       "current_results",
    "social_value"};

[[nodiscard]] bool validIdentifier(std::string_view value) {
    return !value.empty() && value.size() <= 128U &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
           });
}

[[nodiscard]] bool validText(const std::string& value) {
    return !util::trim(value).empty() && value.size() <= kMaxFieldLength;
}

[[nodiscard]] Result<void> validateRole(const JsonValue& value, const std::string& key) {
    if (!value.isString() || value.asString().empty() ||
        assetRoleFromString(value.asString()) == AssetRole::Unknown) {
        return Result<void>::failure("条件 " + key + " 包含未知资产角色");
    }
    return Result<void>::success();
}

template <typename Validator>
[[nodiscard]] Result<void> validateNonEmptyArray(const JsonValue& value, const std::string& key,
                                                 Validator validator) {
    if (!value.isArray() || value.asArray().empty() || value.asArray().size() > 256U) {
        return Result<void>::failure("条件 " + key + " 必须是非空且有界的数组");
    }
    for (const auto& item : value.asArray()) {
        const auto checked = validator(item, key);
        if (!checked.ok()) {
            return checked;
        }
    }
    return Result<void>::success();
}

[[nodiscard]] Result<void> validateCondition(const AuditRule& rule) {
    if (!rule.condition.isObject() || rule.condition.asObject().empty()) {
        return Result<void>::failure("规则条件必须是非空对象: " + rule.ruleId);
    }

    bool actionable = false;
    const auto& condition = rule.condition.asObject();
    for (const auto& [key, value] : condition) {
        if (!kConditionKeys.contains(key)) {
            return Result<void>::failure("规则包含不支持的条件 " + key + ": " + rule.ruleId);
        }
        if (kBooleanConditions.contains(key)) {
            if (!value.isBool()) {
                return Result<void>::failure("条件 " + key + " 必须是布尔值: " + rule.ruleId);
            }
            actionable = actionable || value.asBool();
            continue;
        }
        if (key == "minimum_asset_count") {
            const auto number = value.asNumber(-1.0);
            if (!value.isNumber() || !std::isfinite(number) || number < 1.0 ||
                number > 1000000.0 || std::floor(number) != number) {
                return Result<void>::failure("minimum_asset_count 必须是 1..1000000 的整数: " +
                                             rule.ruleId);
            }
            actionable = true;
            continue;
        }
        if (key == "max_ratio") {
            const auto number = value.asNumber(-1.0);
            if (!value.isNumber() || !std::isfinite(number) || number < 0.0 || number > 1.0) {
                return Result<void>::failure("max_ratio 必须位于 0..1: " + rule.ruleId);
            }
            continue;
        }
        if (key == "required_asset" || key == "missing_asset") {
            const auto checked = validateRole(value, key);
            if (!checked.ok()) {
                return Result<void>::failure(checked.error() + ": " + rule.ruleId);
            }
            actionable = true;
            continue;
        }
        if (key == "required_assets" || key == "required_any_assets" ||
            key == "missing_assets" || key == "forbidden_roles") {
            const auto checked = validateNonEmptyArray(value, key, validateRole);
            if (!checked.ok()) {
                return Result<void>::failure(checked.error() + ": " + rule.ruleId);
            }
            actionable = true;
            continue;
        }
        if (key == "required_claim_evidence") {
            const auto checked = validateNonEmptyArray(
                value, key, [](const JsonValue& item, const std::string& name) {
                    if (!item.isString() ||
                        !kClaims.contains(util::lowerAscii(item.asString()))) {
                        return Result<void>::failure("条件 " + name + " 包含未知声明类型");
                    }
                    return Result<void>::success();
                });
            if (!checked.ok()) {
                return Result<void>::failure(checked.error() + ": " + rule.ruleId);
            }
            actionable = true;
            continue;
        }
        if (key == "required_cpir_fields") {
            const auto checked = validateNonEmptyArray(
                value, key, [](const JsonValue& item, const std::string& name) {
                    if (!item.isString() || !kCpirFields.contains(item.asString())) {
                        return Result<void>::failure("条件 " + name + " 包含未知 CPIR 字段");
                    }
                    return Result<void>::success();
                });
            if (!checked.ok()) {
                return Result<void>::failure(checked.error() + ": " + rule.ruleId);
            }
            actionable = true;
        }
    }

    if (condition.contains("max_ratio") &&
        (!condition.contains("vendor_generated_ratio") ||
         !condition.at("vendor_generated_ratio").asBool(false))) {
        return Result<void>::failure("max_ratio 只能修饰 vendor_generated_ratio: " + rule.ruleId);
    }
    if (!actionable) {
        return Result<void>::failure("规则没有可执行的条件: " + rule.ruleId);
    }
    return Result<void>::success();
}

} // namespace

Result<void> RulePackValidator::validate(const std::vector<AuditRule>& rules) const {
    if (rules.empty()) {
        return Result<void>::failure("规则包没有规则");
    }
    if (rules.size() > kMaxRules) {
        return Result<void>::failure("规则数量超过上限");
    }
    std::set<std::string> ids;
    for (const auto& rule : rules) {
        if (!validIdentifier(rule.ruleId) || !validText(rule.name) ||
            !kTracks.contains(rule.track) || !validIdentifier(rule.target) ||
            !validText(rule.description) || !validText(rule.failReason) ||
            !validText(rule.fixTask)) {
            return Result<void>::failure("规则字段不完整: " + rule.ruleId);
        }
        if (!ids.insert(rule.ruleId).second) {
            return Result<void>::failure("规则 ID 重复: " + rule.ruleId);
        }
        const auto condition = validateCondition(rule);
        if (!condition.ok()) {
            return condition;
        }
    }
    return Result<void>::success();
}

} // namespace cc
