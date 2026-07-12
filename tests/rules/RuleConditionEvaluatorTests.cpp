/**
 * @file RuleConditionEvaluatorTests.cpp
 * @brief 规则条件评估器测试。
 */

#include "../TestSupport.hpp"
#include "cc/rules/RuleConditionEvaluator.hpp"

void runRuleConditionEvaluatorTests() {
    cc::AuditRule requiredAssetsRule;
    requiredAssetsRule.condition =
        cc::JsonValue::Object{{"required_assets", cc::JsonValue::Array{"BusinessPlan"}}};

    const auto missingBusinessPlan =
        cc::RuleConditionEvaluator{}.evaluate(requiredAssetsRule, {}, {}, {}, {}, {});
    requireTrue(missingBusinessPlan.failed, "required_assets should fail when role is missing");
    requireTrue(!missingBusinessPlan.missing.empty(),
                "required_assets should explain missing role");

    cc::AuditRule sensitiveRule;
    sensitiveRule.condition = cc::JsonValue::Object{{"forbidden_sensitive_file", true}};

    cc::ProjectAsset secret;
    secret.role = cc::AssetRole::SecretRisk;
    secret.relativePath = "config/.env";
    cc::ProjectInventory inventory;
    inventory.assets = {secret};
    inventory.roleCounts[cc::AssetRole::SecretRisk] = 1;

    const auto sensitiveResult =
        cc::RuleConditionEvaluator{}.evaluate(sensitiveRule, inventory, {}, {}, {}, {});
    requireTrue(sensitiveResult.failed, "forbidden_sensitive_file should fail on secrets");
    requireTrue(!sensitiveResult.evidence.empty(),
                "forbidden_sensitive_file should include file evidence");

    cc::AuditRule deployRule;
    deployRule.condition = cc::JsonValue::Object{{"technical_route_completeness", true}};
    cc::CPIR software;
    software.technicalRoute = "C++/Qt";
    cc::ProjectAsset source;
    source.role = cc::AssetRole::SourceCode;
    cc::ProjectAsset build;
    build.role = cc::AssetRole::BuildSystem;
    cc::ProjectInventory softwareInventory;
    softwareInventory.assets = {source, build};
    softwareInventory.roleCounts[cc::AssetRole::SourceCode] = 1;
    softwareInventory.roleCounts[cc::AssetRole::BuildSystem] = 1;
    const auto missingDeployment =
        cc::RuleConditionEvaluator{}.evaluate(deployRule, softwareInventory, software, {}, {}, {});
    requireTrue(missingDeployment.failed,
                "a build file must not be mistaken for a deployment guide");

    cc::AuditRule evidenceRule;
    evidenceRule.condition =
        cc::JsonValue::Object{{"required_claim_evidence", cc::JsonValue::Array{"CostReduction"}}};
    cc::ProjectClaim costClaim;
    costClaim.claimId = "CLM-COST";
    costClaim.claimType = cc::ClaimType::CostReduction;
    const auto missingCostEvidence =
        cc::RuleConditionEvaluator{}.evaluate(evidenceRule, {}, {}, {costClaim}, {}, {});
    requireTrue(missingCostEvidence.failed,
                "all declared claim types must participate in evidence rules");
    const auto missingRequiredClaim =
        cc::RuleConditionEvaluator{}.evaluate(evidenceRule, {}, {}, {}, {}, {});
    requireTrue(missingRequiredClaim.failed,
                "required claim evidence must fail when the required claim is absent");
}
