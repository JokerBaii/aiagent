/**
 * @file AuditEngine.cpp
 * @brief 核心审计引擎实现。
 */

#include "cc/audit/AuditEngine.hpp"
#include "cc/audit/TrustScoreCalculator.hpp"
#include "cc/claim/ClaimExtractor.hpp"
#include "cc/consistency/ConsistencyChecker.hpp"
#include "cc/cpir/CPIRBuilder.hpp"
#include "cc/cpir/CompetitionTypeDetector.hpp"
#include "cc/evidence/EvidenceMatcher.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/repair/FixTaskGenerator.hpp"
#include "cc/repair/RepairPlanner.hpp"
#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RulePackLoader.hpp"
#include "cc/rules/RulePackValidator.hpp"
#include "cc/text/TextExtractionService.hpp"

namespace cc {

Result<AuditResult> AuditEngine::run(const ProjectContext& context,
                                     const AuditOptions& options) const {
    auto inventory = InventoryEngine{}.build(context);
    if (!inventory.ok()) {
        return Result<AuditResult>::failure(inventory.error());
    }
    auto corpus = TextExtractionService{}.extract(inventory.value());
    if (!corpus.ok()) {
        return Result<AuditResult>::failure(corpus.error());
    }

    const auto type =
        CompetitionTypeDetector{}.detectDetailed(inventory.value(), corpus.value(), options.track);
    auto cpir = CPIRBuilder{}.build(inventory.value(), corpus.value(), type);
    auto claims = ClaimExtractor{}.extract(corpus.value());
    auto matches = EvidenceMatcher{}.match(claims, inventory.value());
    auto issues = ConsistencyChecker{}.check(cpir, inventory.value(), claims);

    auto rules = RulePackLoader{}.loadDirectory(options.rulesDir, type.type);
    if (!rules.ok()) {
        return Result<AuditResult>::failure(rules.error());
    }
    auto valid = RulePackValidator{}.validate(rules.value());
    if (!valid.ok()) {
        return Result<AuditResult>::failure(valid.error());
    }
    auto findings =
        RuleEngine{}.evaluate(rules.value(), inventory.value(), cpir, claims, matches, issues);
    auto score = TrustScoreCalculator{}.calculate(inventory.value(), findings, matches, issues);
    auto tasks = FixTaskGenerator{}.generate(findings, matches);
    auto repairPlan = RepairPlanner{}.plan(tasks, cpir);

    AuditResult result;
    result.context = context;
    result.inventory = inventory.value();
    result.corpus = corpus.value();
    result.cpir = std::move(cpir);
    result.claims = std::move(claims);
    result.evidenceMatches = std::move(matches);
    result.consistencyIssues = std::move(issues);
    result.findings = std::move(findings);
    result.trustScore = std::move(score);
    result.fixTasks = std::move(tasks);
    result.repairPlan = std::move(repairPlan);
    result.toolOutputs = {"inventory_project",  "extract_text",        "detect_competition_type",
                          "build_cpir",         "extract_claims",      "match_evidence",
                          "check_consistency",  "run_rules",           "calculate_trust_score",
                          "generate_fix_tasks", "generate_repair_plan"};
    return Result<AuditResult>::success(std::move(result));
}

} // namespace cc
