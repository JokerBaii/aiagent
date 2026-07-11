#include "../TestSupport.hpp"
#include "cc/agent/WorkspaceEditor.hpp"
#include "cc/audit/AuditEngine.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <cstdlib>

void runWorkspaceEditorTests() {
    const auto root = std::filesystem::temp_directory_path() / "contest-workspace-editor-test";
    const auto project = root / "project";
    const auto workspace = root / "workspace";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(project, error);
    requireTrue(!error, "workspace editor fixture should initialize");
    requireTrue(cc::util::writeTextFile(project / "README.md", "# Demo\nold value\n").ok(),
                "workspace editor markdown fixture should write");
    requireTrue(cc::util::writeTextFile(project / "main.cpp",
                                        "int value = 1;\nint other = 1;\n").ok(),
                "workspace editor source fixture should write");
    requireTrue(cc::util::writeTextFile(project / ".env", "API_KEY=real-secret\n").ok(),
                "workspace editor sensitive fixture should write");

    cc::WorkspaceEditor editor;
    auto edited = editor.applyTextEdit(project, workspace, "README.md", "old value",
                                       "new verified value");
    requireTrue(edited.ok(), "an exact text edit should succeed in the repaired project");
    requireTrue(cc::util::readFileLimited(project / "README.md", 1024U).find("old value") !=
                    std::string::npos,
                "workspace editing must never mutate the original project");
    requireTrue(cc::util::readFileLimited(edited.value().repairedRoot / "README.md", 1024U)
                        .find("new verified value") != std::string::npos,
                "the repaired project should contain the exact replacement");

    auto created = editor.createTextFile(project, workspace, "docs/verification.md",
                                         "# Verification\nRun the documented checks.\n");
    requireTrue(created.ok(), "creating a new repaired-project text file should succeed");
    requireTrue(!std::filesystem::exists(project / "docs/verification.md"),
                "new repaired files must not appear in the original project");

    auto changed = editor.changes(project, workspace);
    requireTrue(changed.ok() && changed.value().size() == 2U,
                "workspace change listing should derive two real changes");
    requireTrue(std::filesystem::is_regular_file(edited.value().patchFile),
                "workspace edits should maintain a combined patch artifact");

    const auto gitRoot = root / "git-check";
    std::filesystem::create_directories(gitRoot, error);
    requireTrue(cc::util::writeTextFile(gitRoot / "README.md", "# Demo\nold value\n").ok(),
                "git patch fixture should write");
    requireTrue(cc::util::writeTextFile(gitRoot / "main.cpp",
                                        "int value = 1;\nint other = 1;\n").ok(),
                "git source fixture should write");
    requireTrue(cc::util::writeTextFile(gitRoot / ".env", "API_KEY=real-secret\n").ok(),
                "git sensitive fixture should write");
    const auto init = "git -C \"" + gitRoot.string() + "\" init -q";
    const auto check = "git -C \"" + gitRoot.string() + "\" apply --check \"" +
                       edited.value().patchFile.string() + "\"";
    requireTrue(std::system(init.c_str()) == 0, "workspace patch test repository should initialize");
    requireTrue(std::system(check.c_str()) == 0,
                "the combined repaired-project patch must pass git apply --check");

    auto sensitive = editor.applyTextEdit(project, workspace, ".env", "real-secret", "redacted");
    requireTrue(!sensitive.ok(), "the agent must not read or edit sensitive project files");
    auto ambiguous = editor.applyTextEdit(project, workspace, "main.cpp", " = 1", " = 2", 1U);
    requireTrue(!ambiguous.ok(), "an unexpected exact-match count must block the edit");

    cc::AuditOptions options;
    options.rulesDir = sourceDir() / "rules";
    auto reAudit = editor.reAudit(project, workspace, options);
    requireTrue(reAudit.ok(), "the repaired project should be accepted by deterministic re-audit");
    requireTrue(reAudit.value().context.unpackStatus == "REPAIRED_WORKSPACE",
                "re-audit must identify its repaired workspace source");

    const auto truthProject = root / "truth-project";
    const auto truthWorkspace = root / "truth-workspace";
    std::filesystem::create_directories(truthProject, error);
    requireTrue(cc::util::writeTextFile(
                    truthProject / "商业计划.md",
                    "# 项目申报书\n目标用户：学生\n已实现营收 50 万元。\n")
                    .ok(),
                "truth fixture should write its original claim");
    cc::ProjectContext baselineContext;
    baselineContext.originalRoot = truthProject;
    baselineContext.inputRoot = truthProject;
    baselineContext.workspaceRoot = truthWorkspace;
    baselineContext.projectName = "truth-project";
    auto baselineAudit = cc::AuditEngine{}.run(baselineContext, options);
    requireTrue(baselineAudit.ok(), "truth fixture baseline audit should complete");
    auto fakeEvidence = editor.createTextFile(
        truthProject, truthWorkspace, "evidence/订单证明.md",
        "订单号 ORDER-2026-001\n已实现营收 50 万元，支付凭证齐全。\n");
    requireTrue(fakeEvidence.ok(), "workspace should allow drafting an evidence-shaped file");
    auto guardedAudit = editor.reAudit(truthProject, truthWorkspace, options,
                                       &baselineAudit.value().context);
    requireTrue(guardedAudit.ok(), "guarded repaired-project audit should complete");
    const auto draftedAsset = std::find_if(
        guardedAudit.value().inventory.assets.begin(),
        guardedAudit.value().inventory.assets.end(), [](const cc::ProjectAsset& asset) {
            return asset.relativePath == "evidence/订单证明.md";
        });
    requireTrue(draftedAsset != guardedAudit.value().inventory.assets.end() &&
                    draftedAsset->workspaceModified && draftedAsset->generated &&
                    draftedAsset->role == cc::AssetRole::Generated,
                "AI-created evidence-shaped files must remain unverified drafts");
    requireTrue(std::none_of(
                    guardedAudit.value().evidenceMatches.begin(),
                    guardedAudit.value().evidenceMatches.end(), [](const cc::EvidenceMatch& match) {
                        return match.status == cc::EvidenceStatus::Supported;
                    }),
                "an AI-created draft must not turn an unsupported claim into supported evidence");
    requireTrue(guardedAudit.value().trustScore.totalScore <=
                    baselineAudit.value().trustScore.totalScore,
                "AI-created evidence must not increase the deterministic trust score");
    requireTrue(guardedAudit.value().context.originalRoot ==
                        baselineAudit.value().context.originalRoot &&
                    guardedAudit.value().context.projectName ==
                        baselineAudit.value().context.projectName,
                "re-audit must preserve original project provenance");

    std::filesystem::remove_all(root, error);
}
