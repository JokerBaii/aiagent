/**
 * @file RulesTests.cpp
 * @brief rules 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RulePackLoader.hpp"
#include "cc/rules/RulePackValidator.hpp"

#include <algorithm>

void runRulesTests() {
    auto rules = cc::RulePackLoader{}.loadFile(sourceDir() / "rules/common_rules.json");
    requireTrue(rules.ok(), "common rules should load");
    auto valid = cc::RulePackValidator{}.validate(rules.value());
    requireTrue(valid.ok(), "common rules should validate");

    cc::AuditRule ratioRule;
    ratioRule.ruleId = "TEST_RATIO";
    ratioRule.name = "ratio";
    ratioRule.track = "common";
    ratioRule.severity = cc::Severity::Warning;
    ratioRule.target = "assets";
    ratioRule.description = "ratio";
    ratioRule.condition =
        cc::JsonValue::Object{{"vendor_generated_ratio", true}, {"max_ratio", 0.2}};
    ratioRule.failReason = "too many generated files";
    ratioRule.fixTask = "remove generated files";

    cc::ProjectInventory inventory;
    cc::ProjectAsset generated;
    generated.role = cc::AssetRole::Generated;
    inventory.assets = {generated, generated, generated};
    inventory.roleCounts[cc::AssetRole::Generated] = 3;
    auto findings = cc::RuleEngine{}.evaluate({ratioRule}, inventory, {}, {}, {}, {});
    requireTrue(!findings.empty(), "vendor_generated_ratio should trigger");
    requireTrue(findings.front().title == "ratio" &&
                    findings.front().reason == "too many generated files" &&
                    findings.front().reason.find("缺失/风险项") == std::string::npos,
                "rule findings must keep structured missing data out of user-facing prose");

    cc::AuditRule unknownCondition = ratioRule;
    unknownCondition.ruleId = "TEST_UNKNOWN_CONDITION";
    unknownCondition.condition = cc::JsonValue::Object{{"invented_condition", true}};
    requireTrue(!cc::RulePackValidator{}.validate({unknownCondition}).ok(),
                "unknown conditions must be rejected instead of silently passing");

    cc::AuditRule invalidThreshold = ratioRule;
    invalidThreshold.ruleId = "TEST_INVALID_THRESHOLD";
    invalidThreshold.condition = cc::JsonValue::Object{{"minimum_asset_count", -1.0}};
    requireTrue(!cc::RulePackValidator{}.validate({invalidThreshold}).ok(),
                "negative asset thresholds must be rejected");

    auto aiRules = cc::RulePackLoader{}.loadDirectory(sourceDir() / "rules",
                                                      cc::CompetitionType::AiApplication);
    requireTrue(aiRules.ok(), "AI track should load its compatible software rule pack");
    cc::CPIR aiProject;
    aiProject.competitionType = cc::CompetitionType::AiApplication;
    const auto aiFindings = cc::RuleEngine{}.evaluate(aiRules.value(), {}, aiProject, {}, {}, {});
    requireTrue(
        std::any_of(aiFindings.begin(), aiFindings.end(),
                    [](const cc::AuditFinding& item) { return item.ruleId == "SOFT_SOURCE_001"; }),
        "software rules must execute for the AI track");
}
