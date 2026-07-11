/**
 * @file AuditTests.cpp
 * @brief audit 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/audit/AuditEngine.hpp"
#include "cc/audit/DiffVerifier.hpp"
#include "cc/audit/TrustScoreCalculator.hpp"
#include "cc/loader/ProjectLoader.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>

void runAuditTests() {
    cc::AuditOptions options;
    options.track = cc::CompetitionType::BusinessInnovation;
    options.rulesDir = sourceDir() / "rules";

    auto context = cc::ProjectLoader{}.load(sourceDir() / "examples/business_bad_case");
    requireTrue(context.ok(), "loader should prepare context for AuditEngine");
    auto result = cc::AuditEngine{}.run(context.value(), options);
    requireTrue(result.ok(), "AuditEngine should run on prepared context");
    requireTrue(result.value().trustScore.totalScore <= 100, "score should be bounded");
    requireTrue(!result.value().findings.empty(), "bad case should trigger findings");
    requireTrue(!result.value().toolOutputs.empty(), "audit result should record tool outputs");
    requireTrue(result.value().cpir.competitionConfidence > 0.0,
                "audit should record competition confidence");

    // 分步流水线应逐步产出真实观察，并得到与批处理一致的最终结果。
    cc::StagedAuditPipeline staged;
    requireTrue(!cc::StagedAuditPipeline::stages().empty(), "staged pipeline should expose stages");
    auto begun = staged.begin(sourceDir() / "examples/business_bad_case", options);
    requireTrue(begun.ok(), "staged pipeline should begin");
    std::size_t stageObservations = 0;
    while (staged.hasNext()) {
        auto step = staged.advance();
        requireTrue(step.ok(), "each staged step should succeed");
        requireTrue(step.value().ok, "each staged observation should be ok on a valid project");
        requireTrue(!step.value().output.at("title").asString().empty(),
                    "staged observation should carry a stage title");
        ++stageObservations;
    }
    requireTrue(stageObservations == cc::StagedAuditPipeline::stages().size(),
                "staged pipeline should emit one observation per stage");
    auto stagedResult = staged.finish();
    requireTrue(stagedResult.ok(), "staged pipeline should finish");
    requireTrue(stagedResult.value().trustScore.totalScore == result.value().trustScore.totalScore,
                "staged and batch pipelines should agree on the trust score");
    requireTrue(stagedResult.value().findings.size() == result.value().findings.size(),
                "staged and batch pipelines should agree on findings count");

    cc::AuditResult oldAudit;
    oldAudit.trustScore.totalScore = 60;
    oldAudit.trustScore.trustDebt = 40;
    oldAudit.trustScore.dimensions["材料完整性"] = 5;
    oldAudit.trustScore.dimensions["项目逻辑自洽性"] = 8;
    oldAudit.evidenceMatches = {{.claimId = "CLM-001", .status = cc::EvidenceStatus::Unsupported}};
    oldAudit.fixTasks = {{.taskId = "FIX-001"}};

    cc::AuditResult newAudit;
    newAudit.trustScore.totalScore = 82;
    newAudit.trustScore.trustDebt = 18;
    newAudit.trustScore.dimensions["材料完整性"] = 12;
    newAudit.trustScore.dimensions["项目逻辑自洽性"] = 14;
    newAudit.evidenceMatches = {{.claimId = "CLM-001", .status = cc::EvidenceStatus::Supported}};

    const auto oldPath = std::filesystem::temp_directory_path() / "contest_old_audit.json";
    const auto newPath = std::filesystem::temp_directory_path() / "contest_new_audit.json";
    requireTrue(cc::JsonReporter{}.write(oldAudit, oldPath).ok(), "old audit json should write");
    requireTrue(cc::JsonReporter{}.write(newAudit, newPath).ok(), "new audit json should write");
    auto diff = cc::DiffVerifier{}.diffFiles(oldPath, newPath);
    requireTrue(diff.ok(), "audit diff should load json reports");
    requireTrue(diff.value().newScore == 82, "audit diff should compare score");
    requireTrue(diff.value().newEvidenceCoverage > diff.value().oldEvidenceCoverage,
                "audit diff should compare evidence coverage");
    requireTrue(diff.value().oldFixTaskCount == 1 && diff.value().newFixTaskCount == 0,
                "audit diff should compare fix task count");
    auto diffJson = cc::auditDiffToJson(diff.value());
    requireTrue(diffJson.at("new_score").asNumber() == 82.0, "audit diff json should export");

    const auto invalidAuditPath =
        std::filesystem::temp_directory_path() / "contest_invalid_audit.json";
    requireTrue(cc::util::writeTextFile(invalidAuditPath, "{}").ok(),
                "invalid audit fixture should write");
    requireTrue(!cc::DiffVerifier{}.diffFiles(invalidAuditPath, newPath).ok(),
                "an object without the audit schema must not produce a zero diff");

    auto largeJson = cc::JsonReporter{}.toJson(oldAudit);
    largeJson.asObject()["unused_padding"] = std::string(5U * 1024U * 1024U, 'x');
    const auto largePath = std::filesystem::temp_directory_path() / "contest_large_audit.json";
    requireTrue(cc::util::writeTextFile(largePath, cc::writeJson(largeJson)).ok(),
                "large valid audit fixture should write");
    requireTrue(cc::DiffVerifier{}.diffFiles(largePath, newPath).ok(),
                "valid reports larger than the former 4 MiB limit must remain comparable");

    cc::EvidenceMatch conflicted{.claimId = "CONFLICT",
                                 .status = cc::EvidenceStatus::Conflicted,
                                 .reason = "证据互相冲突"};
    cc::EvidenceMatch review{.claimId = "REVIEW",
                             .status = cc::EvidenceStatus::NeedReview,
                             .reason = "材料不可读"};
    const auto evidenceScore = cc::TrustScoreCalculator{}.calculate({}, {}, {conflicted, review}, {});
    requireTrue(evidenceScore.totalScore < 100,
                "conflicted and need-review evidence must affect the deterministic score");

    const auto unreadableRoot =
        std::filesystem::temp_directory_path() / "contest-unreadable-declaration-test";
    std::error_code cleanupError;
    std::filesystem::remove_all(unreadableRoot, cleanupError);
    std::filesystem::create_directories(unreadableRoot, cleanupError);
    requireTrue(!cleanupError, "unreadable declaration fixture should initialize");
    requireTrue(cc::util::writeTextFile(unreadableRoot / "申报书.pdf",
                                        "%PDF-1.7\n% scan without readable streams\n%%EOF\n")
                    .ok(),
                "unreadable declaration fixture should write");
    cc::AuditOptions unreadableOptions;
    unreadableOptions.rulesDir = sourceDir() / "rules";
    auto unreadableContext = cc::ProjectLoader{}.load(unreadableRoot);
    requireTrue(unreadableContext.ok(), "unreadable declaration should still import safely");
    auto unreadableAudit = cc::AuditEngine{}.run(unreadableContext.value(), unreadableOptions);
    requireTrue(unreadableAudit.ok(), "unreadable declaration audit should complete");
    requireTrue(unreadableAudit.value().trustScore.totalScore < 100,
                "an unreadable declaration must never receive a perfect score");
    requireTrue(std::any_of(unreadableAudit.value().findings.begin(),
                            unreadableAudit.value().findings.end(), [](const auto& finding) {
                                return finding.ruleId.starts_with("TEXT_EXTRACTION_REVIEW_");
                            }),
                "unreadable documents must create an explicit extraction finding");
    std::filesystem::remove_all(unreadableRoot, cleanupError);
}
