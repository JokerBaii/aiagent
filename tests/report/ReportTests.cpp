/**
 * @file ReportTests.cpp
 * @brief report 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/report/JsonReporter.hpp"
#include "cc/report/MarkdownReporter.hpp"

void runReportTests() {
    cc::AuditResult result;
    result.context.originalRoot = "original";
    result.context.inputRoot = "workspace/input";
    result.context.workspaceRoot = "workspace";
    result.context.unpackStatus = "DIRECTORY_COPIED_TO_WORKSPACE";
    result.cpir.projectName = "Demo";
    result.cpir.targetUser = "学生";
    result.cpir.painPoint = "材料分散";
    result.cpir.competitionConfidence = 1.0;
    result.cpir.competitionReason = "测试指定";
    cc::ProjectAsset asset;
    asset.relativePath = "src/main.cpp";
    asset.fileName = "main.cpp";
    asset.mime = "text/plain";
    asset.language = "cpp";
    asset.importance = 3;
    asset.generated = true;
    asset.vendored = true;
    result.inventory.assets = {asset};
    result.trustScore.totalScore = 88;
    result.trustScore.dimensions["材料完整性"] = 12;
    result.trustScore.dimensions["项目逻辑自洽性"] = 15;
    result.evidenceMatches = {{.claimId = "CLM-001", .status = cc::EvidenceStatus::Supported},
                              {.claimId = "CLM-002", .status = cc::EvidenceStatus::Partial}};
    result.consistencyIssues = {{.issueId = "CONS-001",
                                 .severity = cc::Severity::Warning,
                                 .description = "README 与源码结构不一致",
                                 .affectedFiles = {"README.md"},
                                 .fixSuggestion = "同步 README 功能清单"}};
    result.toolOutputs = {"inventory_project", "run_rules"};
    result.findings.push_back({"BLOCK", cc::Severity::Blocker, "阻断", "原因", {}, {}, "修复"});
    result.fixTasks.push_back({.taskId = "FIX-P0", .priority = "P0"});
    result.fixTasks.push_back({.taskId = "FIX-P0-2", .priority = "P0"});
    cc::AuditDiff diff;
    diff.oldScore = 70;
    diff.newScore = 88;
    diff.summary = "评分提升";
    auto json = cc::JsonReporter{}.toJson(result, &diff);
    requireTrue(json.at("summary").at("project_name").asString() == "Demo", "json report mismatch");
    requireTrue(json.at("context").at("unpack_status").asString() ==
                    "DIRECTORY_COPIED_TO_WORKSPACE",
                "json report should include project context");
    requireTrue(json.at("cpir").at("pain_point").asString() == "材料分散",
                "json report should include full CPIR fields");
    requireTrue(json.at("consistency_issues").at(0).at("issue_id").asString() == "CONS-001",
                "json report should include consistency issues");
    requireTrue(json.at("tool_outputs").at(0).asString() == "inventory_project",
                "json report should include tool outputs");
    const auto& exportedAsset = json.at("inventory").at("assets").at(0);
    requireTrue(exportedAsset.at("mime").asString() == "text/plain",
                "json report should include asset mime");
    requireTrue(exportedAsset.at("language").asString() == "cpp",
                "json report should include asset language");
    requireTrue(exportedAsset.at("importance").asNumber() == 3.0,
                "json report should include asset importance");
    requireTrue(exportedAsset.at("generated").asBool(), "json report should include generated");
    requireTrue(exportedAsset.at("third_party").asBool(),
                "json report should include third party flag");
    requireTrue(json.at("summary").at("evidence_coverage").asNumber() == 75.0,
                "json report should include evidence coverage");
    requireTrue(json.at("summary").at("blocker_count").asNumber() == 2.0,
                "summary blocker count should include distinct P0 obligations");
    requireTrue(json.at("summary").at("rule_blocker_count").asNumber() == 1.0,
                "summary should preserve the raw rule blocker count");
    requireTrue(json.at("summary").at("material_completeness").asNumber() == 12.0,
                "json report should include material completeness");
    requireTrue(json.at("audit_diff").at("new_score").asNumber() == 88.0,
                "json report should bind an available audit diff");
    const auto markdown = cc::MarkdownReporter{}.render(result, &diff);
    requireTrue(markdown.find("## 资产清单") != std::string::npos,
                "markdown report should include assets");
    requireTrue(markdown.find("## CPIR 项目中间表示") != std::string::npos,
                "markdown report should include CPIR");
    requireTrue(markdown.find("## 材料一致性风险") != std::string::npos,
                "markdown report should include consistency");
    requireTrue(markdown.find("## 声明—证据匹配") != std::string::npos,
                "markdown report should include claim evidence matches");
    requireTrue(markdown.find("## 二次审计差分") != std::string::npos,
                "markdown report should include audit diff section");
    requireTrue(markdown.find("70 -> 88") != std::string::npos,
                "markdown report should render the bound audit diff");
}
