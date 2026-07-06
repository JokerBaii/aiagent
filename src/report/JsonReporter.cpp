/**
 * @file JsonReporter.cpp
 * @brief JSON 审计包导出实现。
 */

#include "cc/report/JsonReporter.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/JsonUtil.hpp"

namespace cc {
namespace {

[[nodiscard]] int dimensionScore(const TrustScore& score, const std::string& dimension) {
    const auto iter = score.dimensions.find(dimension);
    return iter == score.dimensions.end() ? 0 : iter->second;
}

[[nodiscard]] double evidenceCoverage(const std::vector<EvidenceMatch>& matches) {
    if (matches.empty()) {
        return 0.0;
    }
    double covered = 0.0;
    for (const auto& match : matches) {
        if (match.status == EvidenceStatus::Supported) {
            covered += 1.0;
        } else if (match.status == EvidenceStatus::Partial) {
            covered += 0.5;
        }
    }
    return covered * 100.0 / static_cast<double>(matches.size());
}

} // namespace

JsonValue contextToJson(const ProjectContext& context) {
    return JsonValue::Object{{"original_root", util::pathString(context.originalRoot)},
                             {"input_root", util::pathString(context.inputRoot)},
                             {"workspace_root", util::pathString(context.workspaceRoot)},
                             {"session_id", context.sessionId},
                             {"project_name", context.projectName},
                             {"unpack_status", context.unpackStatus},
                             {"archive_input", context.archiveInput},
                             {"input_files", util::pathArrayToJson(context.inputFiles)},
                             {"warnings", util::stringArrayToJson(context.warnings)}};
}

JsonValue inventoryToJson(const ProjectInventory& inventory) {
    JsonValue::Array assets;
    for (const auto& asset : inventory.assets) {
        assets.emplace_back(
            JsonValue::Object{{"path", util::pathString(asset.relativePath)},
                              {"file_name", asset.fileName},
                              {"extension", asset.extension},
                              {"size_bytes", static_cast<double>(asset.sizeBytes)},
                              {"format", asset.format},
                              {"mime", asset.mime},
                              {"language", asset.language},
                              {"role", toString(asset.role)},
                              {"importance", asset.importance},
                              {"auditable", asset.auditable},
                              {"generated", asset.generated},
                              {"third_party", asset.vendored},
                              {"sensitive", asset.sensitive},
                              {"risk_flags", util::stringArrayToJson(asset.riskFlags)}});
    }
    JsonValue::Object roleCounts;
    for (const auto& [role, count] : inventory.roleCounts) {
        roleCounts[toString(role)] = static_cast<double>(count);
    }
    return JsonValue::Object{{"root", util::pathString(inventory.root)},
                             {"asset_count", static_cast<double>(inventory.assets.size())},
                             {"role_counts", JsonValue{roleCounts}},
                             {"warnings", util::stringArrayToJson(inventory.warnings)},
                             {"assets", JsonValue{assets}}};
}

JsonValue corpusToJson(const std::vector<TextDocument>& corpus) {
    JsonValue::Array array;
    for (const auto& document : corpus) {
        array.emplace_back(
            JsonValue::Object{{"source_file", util::pathString(document.sourceFile)},
                              {"title", document.title},
                              {"status", document.status},
                              {"text_size", static_cast<double>(document.text.size())}});
    }
    return JsonValue{array};
}

JsonValue cpirToJson(const CPIR& cpir) {
    return JsonValue::Object{{"project_name", cpir.projectName},
                             {"competition_type", toString(cpir.competitionType)},
                             {"competition_confidence", cpir.competitionConfidence},
                             {"competition_reason", cpir.competitionReason},
                             {"track", cpir.track},
                             {"target_user", cpir.targetUser},
                             {"pain_point", cpir.painPoint},
                             {"solution", cpir.solution},
                             {"product_or_service", cpir.productOrService},
                             {"technical_route", cpir.technicalRoute},
                             {"business_model", cpir.businessModel},
                             {"market_analysis", cpir.marketAnalysis},
                             {"competitor_analysis", cpir.competitorAnalysis},
                             {"financial_projection", cpir.financialProjection},
                             {"team_structure", cpir.teamStructure},
                             {"current_results", cpir.currentResults},
                             {"social_value", cpir.socialValue},
                             {"missing_fields", util::stringArrayToJson(cpir.missingFields)},
                             {"risk_items", util::stringArrayToJson(cpir.riskItems)}};
}

JsonValue claimsToJson(const std::vector<ProjectClaim>& claims) {
    JsonValue::Array array;
    for (const auto& claim : claims) {
        array.emplace_back(JsonValue::Object{{"claim_id", claim.claimId},
                                             {"claim_text", claim.claimText},
                                             {"claim_type", toString(claim.claimType)},
                                             {"source_file", util::pathString(claim.sourceFile)},
                                             {"confidence", claim.confidence},
                                             {"initial_risk", claim.initialRisk}});
    }
    return JsonValue{array};
}

JsonValue evidenceToJson(const std::vector<EvidenceMatch>& matches) {
    JsonValue::Array array;
    for (const auto& match : matches) {
        array.emplace_back(
            JsonValue::Object{{"claim_id", match.claimId},
                              {"status", toString(match.status)},
                              {"evidence_files", util::pathArrayToJson(match.evidenceFiles)},
                              {"missing_evidence", util::stringArrayToJson(match.missingEvidence)},
                              {"reason", match.reason}});
    }
    return JsonValue{array};
}

JsonValue consistencyToJson(const std::vector<ConsistencyIssue>& issues) {
    JsonValue::Array array;
    for (const auto& issue : issues) {
        array.emplace_back(
            JsonValue::Object{{"issue_id", issue.issueId},
                              {"severity", toString(issue.severity)},
                              {"description", issue.description},
                              {"affected_files", util::pathArrayToJson(issue.affectedFiles)},
                              {"fix_suggestion", issue.fixSuggestion}});
    }
    return JsonValue{array};
}

JsonValue findingsToJson(const std::vector<AuditFinding>& findings) {
    JsonValue::Array array;
    for (const auto& finding : findings) {
        array.emplace_back(JsonValue::Object{
            {"rule_id", finding.ruleId},
            {"severity", toString(finding.severity)},
            {"title", finding.title},
            {"reason", finding.reason},
            {"evidence", util::pathArrayToJson(finding.evidence)},
            {"missing_evidence", util::stringArrayToJson(finding.missingEvidence)},
            {"fix_suggestion", finding.fixSuggestion}});
    }
    return JsonValue{array};
}

JsonValue fixTasksToJson(const std::vector<FixTask>& tasks) {
    JsonValue::Array array;
    for (const auto& task : tasks) {
        array.emplace_back(
            JsonValue::Object{{"task_id", task.taskId},
                              {"title", task.title},
                              {"priority", task.priority},
                              {"reason", task.reason},
                              {"required_material", util::stringArrayToJson(task.requiredMaterial)},
                              {"affected_rules", util::stringArrayToJson(task.affectedRules)},
                              {"related_files", util::pathArrayToJson(task.relatedFiles)}});
    }
    return JsonValue{array};
}

JsonValue trustScoreToJson(const TrustScore& score) {
    JsonValue::Object dimensions;
    for (const auto& [name, value] : score.dimensions) {
        dimensions[name] = value;
    }
    JsonValue::Array penalties;
    for (const auto& penalty : score.penalties) {
        penalties.emplace_back(JsonValue::Object{{"rule_id", penalty.ruleId},
                                                 {"points", penalty.points},
                                                 {"dimension", penalty.dimension},
                                                 {"reason", penalty.reason}});
    }
    return JsonValue::Object{{"total_score", score.totalScore},
                             {"trust_debt", score.trustDebt},
                             {"dimensions", JsonValue{dimensions}},
                             {"penalties", JsonValue{penalties}}};
}

JsonValue auditDiffToJson(const AuditDiff& diff) {
    return JsonValue::Object{{"old_score", diff.oldScore},
                             {"new_score", diff.newScore},
                             {"old_trust_debt", diff.oldTrustDebt},
                             {"new_trust_debt", diff.newTrustDebt},
                             {"old_blockers", diff.oldBlockers},
                             {"new_blockers", diff.newBlockers},
                             {"old_warnings", diff.oldWarnings},
                             {"new_warnings", diff.newWarnings},
                             {"old_evidence_coverage", diff.oldEvidenceCoverage},
                             {"new_evidence_coverage", diff.newEvidenceCoverage},
                             {"old_material_completeness", diff.oldMaterialCompleteness},
                             {"new_material_completeness", diff.newMaterialCompleteness},
                             {"old_consistency_score", diff.oldConsistencyScore},
                             {"new_consistency_score", diff.newConsistencyScore},
                             {"old_fix_task_count", diff.oldFixTaskCount},
                             {"new_fix_task_count", diff.newFixTaskCount},
                             {"summary", diff.summary}};
}

JsonValue JsonReporter::toJson(const AuditResult& result) const {
    int blockerCount = 0;
    int warningCount = 0;
    for (const auto& finding : result.findings) {
        blockerCount += finding.severity == Severity::Blocker ? 1 : 0;
        warningCount += finding.severity == Severity::Warning ? 1 : 0;
    }
    return JsonValue::Object{
        {"summary",
         JsonValue::Object{
             {"project_name", result.cpir.projectName},
             {"competition_type", toString(result.cpir.competitionType)},
             {"asset_count", static_cast<double>(result.inventory.assets.size())},
             {"total_score", result.trustScore.totalScore},
             {"trust_debt", result.trustScore.trustDebt},
             {"blocker_count", blockerCount},
             {"warning_count", warningCount},
             {"evidence_coverage", evidenceCoverage(result.evidenceMatches)},
             {"material_completeness", dimensionScore(result.trustScore, "材料完整性")},
             {"consistency_score", dimensionScore(result.trustScore, "项目逻辑自洽性")},
             {"fix_task_count", static_cast<double>(result.fixTasks.size())}}},
        {"context", contextToJson(result.context)},
        {"inventory", inventoryToJson(result.inventory)},
        {"text_corpus", corpusToJson(result.corpus)},
        {"cpir", cpirToJson(result.cpir)},
        {"claims", claimsToJson(result.claims)},
        {"evidence_matches", evidenceToJson(result.evidenceMatches)},
        {"consistency_issues", consistencyToJson(result.consistencyIssues)},
        {"findings", findingsToJson(result.findings)},
        {"trust_score", trustScoreToJson(result.trustScore)},
        {"fix_tasks", fixTasksToJson(result.fixTasks)},
        {"tool_outputs", util::stringArrayToJson(result.toolOutputs)},
        {"audit_diff", JsonValue{nullptr}},
        {"repair_plan", JsonValue::Object{{"markdown", result.repairPlan.markdown},
                                          {"diff", result.repairPlan.diffText}}}};
}

Result<void> JsonReporter::write(const AuditResult& result,
                                 const std::filesystem::path& output) const {
    return util::writeTextFile(output, writeJson(toJson(result), 2) + "\n");
}

} // namespace cc
