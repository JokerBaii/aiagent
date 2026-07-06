/**
 * @file RuleConditionEvaluator.cpp
 * @brief JSON 规则条件的可解释评估实现。
 */

#include "cc/rules/RuleConditionEvaluator.hpp"
#include "cc/evidence/EvidenceMatcher.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/util/JsonUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <utility>

namespace cc {
namespace {

struct ConditionContext {
    const JsonValue::Object* condition;
    const ProjectInventory* inventory;
    const CPIR* cpir;
    const std::vector<ProjectClaim>* claims;
    const std::vector<EvidenceMatch>* matches;
    const std::vector<ConsistencyIssue>* issues;
};

[[nodiscard]] const JsonValue* objectValue(const JsonValue::Object& object,
                                           const std::string& key) {
    const auto iter = object.find(key);
    return iter == object.end() ? nullptr : &iter->second;
}

[[nodiscard]] bool flagEnabled(const JsonValue::Object& condition, const std::string& key) {
    const auto* value = objectValue(condition, key);
    return value != nullptr && value->asBool(false);
}

void addMissing(RuleConditionResult& result, std::string item) {
    result.failed = true;
    result.missing.emplace_back(std::move(item));
}

void addEvidence(RuleConditionResult& result, const std::vector<std::filesystem::path>& files) {
    result.failed = true;
    result.evidence.insert(result.evidence.end(), files.begin(), files.end());
}

[[nodiscard]] ClaimType claimTypeFromString(const std::string& value) {
    const auto lower = util::lowerAscii(value);
    if (lower == "usertraction") {
        return ClaimType::UserTraction;
    }
    if (lower == "marketscale") {
        return ClaimType::MarketScale;
    }
    if (lower == "technicalcapability") {
        return ClaimType::TechnicalCapability;
    }
    if (lower == "businessmodel") {
        return ClaimType::BusinessModel;
    }
    if (lower == "revenue") {
        return ClaimType::Revenue;
    }
    if (lower == "researchresult") {
        return ClaimType::ResearchResult;
    }
    if (lower == "socialimpact") {
        return ClaimType::SocialImpact;
    }
    if (lower == "deployment") {
        return ClaimType::Deployment;
    }
    return ClaimType::Unknown;
}

[[nodiscard]] std::vector<AssetRole> roleList(const JsonValue* value) {
    std::vector<AssetRole> roles;
    if (value == nullptr || !value->isArray()) {
        return roles;
    }
    for (const auto& item : value->asArray()) {
        if (item.isString()) {
            roles.emplace_back(assetRoleFromString(item.asString()));
        }
    }
    return roles;
}

[[nodiscard]] bool hasClaim(ClaimType type, const std::vector<ProjectClaim>& claims) {
    return std::any_of(claims.begin(), claims.end(),
                       [&](const ProjectClaim& claim) { return claim.claimType == type; });
}

[[nodiscard]] bool supported(ClaimType type, const std::vector<ProjectClaim>& claims,
                             const std::vector<EvidenceMatch>& matches) {
    for (const auto& claim : claims) {
        if (claim.claimType != type) {
            continue;
        }
        const auto iter =
            std::find_if(matches.begin(), matches.end(), [&](const EvidenceMatch& match) {
                return match.claimId == claim.claimId &&
                       (match.status == EvidenceStatus::Supported ||
                        match.status == EvidenceStatus::Partial);
            });
        if (iter != matches.end()) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string cpirField(const CPIR& cpir, const std::string& field) {
    if (field == "target_user") {
        return cpir.targetUser;
    }
    if (field == "business_model") {
        return cpir.businessModel;
    }
    if (field == "financial_projection") {
        return cpir.financialProjection;
    }
    if (field == "technical_route") {
        return cpir.technicalRoute;
    }
    if (field == "market_analysis") {
        return cpir.marketAnalysis;
    }
    if (field == "social_value") {
        return cpir.socialValue;
    }
    return {};
}

[[nodiscard]] bool hasAnyRole(const ProjectInventory& inventory,
                              const std::vector<AssetRole>& roles) {
    return std::any_of(roles.begin(), roles.end(),
                       [&](AssetRole role) { return hasRole(inventory, role); });
}

void evaluateRequiredRoles(const ConditionContext& context, RuleConditionResult& result,
                           const std::string& key) {
    for (const auto role : roleList(objectValue(*context.condition, key))) {
        if (!hasRole(*context.inventory, role)) {
            addMissing(result, toString(role));
        }
    }
}

void evaluateRequiredAnyAssets(const ConditionContext& context, RuleConditionResult& result) {
    const auto anyRoles = roleList(objectValue(*context.condition, "required_any_assets"));
    if (!anyRoles.empty() && !hasAnyRole(*context.inventory, anyRoles)) {
        addMissing(result, "至少一种必需材料");
    }
}

void evaluateMinimumAssetCount(const ConditionContext& context, RuleConditionResult& result) {
    const auto* minAssetCount = objectValue(*context.condition, "minimum_asset_count");
    if (minAssetCount == nullptr || !minAssetCount->isNumber()) {
        return;
    }

    const auto expected = static_cast<std::size_t>(minAssetCount->asNumber(0.0));
    if (context.inventory->assets.size() < expected) {
        addMissing(result, "材料数量至少 " +
                               std::to_string(static_cast<int>(minAssetCount->asNumber(0.0))));
    }
}

void evaluateForbiddenRoles(const ConditionContext& context, RuleConditionResult& result) {
    for (const auto role : roleList(objectValue(*context.condition, "forbidden_roles"))) {
        const auto files = filesWithRole(*context.inventory, role);
        if (!files.empty()) {
            addEvidence(result, files);
            result.missing.emplace_back("移除 " + toString(role));
        }
    }
}

void evaluateForbiddenSensitiveFiles(const ConditionContext& context, RuleConditionResult& result) {
    if (!flagEnabled(*context.condition, "forbidden_sensitive_file") ||
        !hasRole(*context.inventory, AssetRole::SecretRisk)) {
        return;
    }

    addEvidence(result, filesWithRole(*context.inventory, AssetRole::SecretRisk));
    result.missing.emplace_back("移除敏感文件");
}

void evaluateRequiredClaimEvidence(const ConditionContext& context, RuleConditionResult& result) {
    const auto* claimValue = objectValue(*context.condition, "required_claim_evidence");
    for (const auto& claimName :
         util::jsonStringArray(claimValue == nullptr ? JsonValue{} : *claimValue)) {
        const auto type = claimTypeFromString(claimName);
        if (hasClaim(type, *context.claims) &&
            !supported(type, *context.claims, *context.matches)) {
            result.failed = true;
            const auto needed = missingEvidenceForClaim(type);
            result.missing.insert(result.missing.end(), needed.begin(), needed.end());
        }
    }
}

void evaluateRequiredCpirFields(const ConditionContext& context, RuleConditionResult& result) {
    const auto* fieldsValue = objectValue(*context.condition, "required_cpir_fields");
    for (const auto& field :
         util::jsonStringArray(fieldsValue == nullptr ? JsonValue{} : *fieldsValue)) {
        if (cpirField(*context.cpir, field).empty()) {
            addMissing(result, "CPIR 字段: " + field);
        }
    }
}

void evaluateConsistencyIssues(const ConditionContext& context, RuleConditionResult& result) {
    if (!flagEnabled(*context.condition, "consistency_check") || context.issues->empty()) {
        return;
    }

    result.failed = true;
    for (const auto& issue : *context.issues) {
        result.missing.emplace_back(issue.issueId);
    }
}

void evaluateDocCodeSupport(const ConditionContext& context, RuleConditionResult& result) {
    const std::vector<AssetRole> supportRoles{AssetRole::BuildSystem, AssetRole::DependencyManifest,
                                              AssetRole::DeploymentDoc};
    if (flagEnabled(*context.condition, "doc_code_support") &&
        hasRole(*context.inventory, AssetRole::SourceCode) &&
        !hasAnyRole(*context.inventory, supportRoles)) {
        addMissing(result, "源码缺少构建入口、依赖清单或部署说明支撑");
    }
}

void evaluateBusinessCompleteness(const ConditionContext& context, RuleConditionResult& result) {
    if (flagEnabled(*context.condition, "business_model_completeness") &&
        (context.cpir->targetUser.empty() || context.cpir->businessModel.empty() ||
         context.cpir->financialProjection.empty() ||
         !hasRole(*context.inventory, AssetRole::BusinessPlan) ||
         !hasRole(*context.inventory, AssetRole::FinancialPlan))) {
        addMissing(result, "商业闭环字段或材料不完整");
    }
}

void evaluateTechnicalCompleteness(const ConditionContext& context, RuleConditionResult& result) {
    const std::vector<AssetRole> supportRoles{AssetRole::BuildSystem, AssetRole::DependencyManifest,
                                              AssetRole::DeploymentDoc};
    if (flagEnabled(*context.condition, "technical_route_completeness") &&
        (context.cpir->technicalRoute.empty() ||
         !hasRole(*context.inventory, AssetRole::SourceCode) ||
         !hasAnyRole(*context.inventory, supportRoles))) {
        addMissing(result, "技术路线、源码或复现入口不完整");
    }
}

void evaluateResearchReproducibility(const ConditionContext& context, RuleConditionResult& result) {
    if (flagEnabled(*context.condition, "research_reproducibility") &&
        (!hasRole(*context.inventory, AssetRole::ExperimentData) ||
         !supported(ClaimType::ResearchResult, *context.claims, *context.matches))) {
        addMissing(result, "实验数据、评价指标或研究结论证据不足");
    }
}

void evaluateSocialImpactEvidence(const ConditionContext& context, RuleConditionResult& result) {
    if (flagEnabled(*context.condition, "social_impact_evidence") &&
        (!hasRole(*context.inventory, AssetRole::SocialPracticeProof) ||
         !supported(ClaimType::SocialImpact, *context.claims, *context.matches))) {
        addMissing(result, "社会实践过程记录或影响数据不足");
    }
}

void evaluateVendorGeneratedRatio(const ConditionContext& context, RuleConditionResult& result) {
    if (!flagEnabled(*context.condition, "vendor_generated_ratio") ||
        context.inventory->assets.empty()) {
        return;
    }

    const auto risky = countRole(*context.inventory, AssetRole::Generated) +
                       countRole(*context.inventory, AssetRole::Vendored);
    const auto ratio =
        static_cast<double>(risky) / static_cast<double>(context.inventory->assets.size());
    const auto* maxRatio = objectValue(*context.condition, "max_ratio");
    if (ratio > (maxRatio == nullptr ? 0.4 : maxRatio->asNumber(0.4))) {
        addMissing(result, "生成物或第三方依赖占比过高");
    }
}

} // namespace

RuleConditionResult
RuleConditionEvaluator::evaluate(const AuditRule& rule, const ProjectInventory& inventory,
                                 const CPIR& cpir, const std::vector<ProjectClaim>& claims,
                                 const std::vector<EvidenceMatch>& matches,
                                 const std::vector<ConsistencyIssue>& issues) const {
    RuleConditionResult result;
    const ConditionContext context{.condition = &rule.condition.asObject(),
                                   .inventory = &inventory,
                                   .cpir = &cpir,
                                   .claims = &claims,
                                   .matches = &matches,
                                   .issues = &issues};

    evaluateRequiredRoles(context, result, "required_assets");
    evaluateRequiredRoles(context, result, "missing_assets");
    evaluateRequiredAnyAssets(context, result);
    evaluateMinimumAssetCount(context, result);
    evaluateForbiddenRoles(context, result);
    evaluateForbiddenSensitiveFiles(context, result);
    evaluateRequiredClaimEvidence(context, result);
    evaluateRequiredCpirFields(context, result);
    evaluateConsistencyIssues(context, result);
    evaluateDocCodeSupport(context, result);
    evaluateBusinessCompleteness(context, result);
    evaluateTechnicalCompleteness(context, result);
    evaluateResearchReproducibility(context, result);
    evaluateSocialImpactEvidence(context, result);
    evaluateVendorGeneratedRatio(context, result);

    return result;
}

} // namespace cc
