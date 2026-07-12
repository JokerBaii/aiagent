/**
 * @file RepairTests.cpp
 * @brief repair 模块测试。
 */

#include "../TestSupport.hpp"
#include "cc/repair/FixTaskGenerator.hpp"
#include "cc/repair/RepairPlanner.hpp"
#include "cc/util/FileUtil.hpp"

#include <cstdlib>

void runRepairTests() {
    cc::AuditFinding finding{"RULE_1",  cc::Severity::Blocker, "失败", "缺材料", {}, {"申报书"},
                             "补申报书"};
    auto tasks = cc::FixTaskGenerator{}.generate({finding}, {});
    auto plan = cc::RepairPlanner{}.plan(tasks, {});
    requireTrue(!tasks.empty(), "fix task should be generated");
    requireTrue(plan.markdown.find("不生成虚假") != std::string::npos, "repair boundary missing");
    requireTrue(plan.diffText.find("repaired/PROJECT_TRUST_FIX_TASKS.md") != std::string::npos,
                "repair diff should be diff-first");
    requireTrue(plan.diffText.find("@@ -0,0 +1,") != std::string::npos,
                "repair diff must include a valid unified-diff hunk");

    const auto gitRoot = std::filesystem::temp_directory_path() / "contest-repair-diff-check";
    std::error_code error;
    std::filesystem::remove_all(gitRoot, error);
    std::filesystem::create_directories(gitRoot, error);
    requireTrue(!error, "repair diff test repository should be created");
    const auto patchPath = gitRoot / "repair.patch";
    requireTrue(cc::util::writeTextFile(patchPath, plan.diffText).ok(),
                "repair patch fixture should write");
    const auto initCommand = "git -C \"" + gitRoot.string() + "\" init -q";
    const auto checkCommand =
        "git -C \"" + gitRoot.string() + "\" apply --check \"" + patchPath.string() + "\"";
    requireTrue(std::system(initCommand.c_str()) == 0, "git test repository should initialize");
    requireTrue(std::system(checkCommand.c_str()) == 0,
                "generated repair diff must pass git apply --check");
    std::filesystem::remove_all(gitRoot, error);

    cc::EvidenceMatch duplicateA{.claimId = "CLM-001",
                                 .status = cc::EvidenceStatus::Unsupported,
                                 .missingEvidence = {"原始调研数据"},
                                 .reason = "缺少数据"};
    cc::EvidenceMatch duplicateB = duplicateA;
    duplicateB.claimId = "CLM-002";
    const auto grouped = cc::FixTaskGenerator{}.generate({}, {duplicateA, duplicateB});
    requireTrue(grouped.size() == 1U, "equivalent evidence gaps should become one actionable task");
    requireTrue(grouped.front().affectedRules.size() == 2U,
                "a grouped task must retain every affected claim");

    cc::AuditFinding infoFinding{"INFO_RULE", cc::Severity::Info, "提示", "说明", {},
                                 {},          "稍后处理"};
    const auto infoTasks = cc::FixTaskGenerator{}.generate({infoFinding}, {});
    requireTrue(infoTasks.front().priority == "P2", "informational findings must not become P1");
}
