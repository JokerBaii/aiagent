/**
 * @file AgentTests.cpp
 * @brief agent 模块测试。
 */

#include "../TestSupport.hpp"
#include "../ZipFixture.hpp"
#include "cc/agent/AgentCommandRouter.hpp"
#include "cc/agent/AgentFilePolicy.hpp"
#include "cc/agent/AgentPermissionPolicy.hpp"
#include "cc/agent/AgentRuntime.hpp"
#include "cc/agent/AuditSessionStore.hpp"
#include "cc/agent/PermissionGate.hpp"
#include "cc/agent/ProjectMemory.hpp"
#include "cc/agent/StagedAuditPipeline.hpp"
#include "cc/agent/ToolRegistry.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/loader/ProjectLoader.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>

void runAgentTests() {
    const auto help = cc::agentCommandHelpText();
    requireTrue(help.find("/plan") != std::string::npos &&
                    help.find("/optimize") != std::string::npos &&
                    help.find("/help") != std::string::npos,
                "command help must stay aligned with the public composer commands");
    const std::string invalidUtf8{"ok\xFFtext", 7U};
    requireTrue(cc::agent_file_policy::sanitizeUtf8(invalidUtf8) == "ok?text",
                "agent file policy should sanitize invalid UTF-8 before JSON output");
    requireTrue(cc::agent_file_policy::truncateText("中文材料", 4U) == "中\n...[已截断]",
                "agent file policy should truncate on a UTF-8 boundary");

    cc::PermissionGate gate;
    requireTrue(gate.isAllowed(cc::ToolPermission::ReadProjectFiles), "read should be allowed");
    requireTrue(!gate.isAllowed(cc::ToolPermission::WriteWorkspace),
                "workspace writes should require a task capability snapshot");
    requireTrue(!gate.isAllowed(cc::ToolPermission::NetworkAccess),
                "network should be denied by default");
    requireTrue(!gate.isAllowed(cc::ToolPermission::LLMAccess),
                "llm access should be denied by default");
    cc::AgentRunRequest permissionRequest;
    requireTrue(!cc::AgentPermissionPolicy{}
                     .authorize(permissionRequest, cc::ToolPermission::WriteWorkspace)
                     .ok(),
                "request capability snapshot should deny workspace writes by default");
    permissionRequest.allowWriteWorkspace = true;
    requireTrue(cc::AgentPermissionPolicy{}
                    .authorize(permissionRequest, cc::ToolPermission::WriteWorkspace)
                    .ok(),
                "request capability snapshot should explicitly allow workspace writes");
    cc::AgentRunRequest localOptimizeRequest;
    localOptimizeRequest.requireWorkspaceChanges = true;
    requireTrue(!cc::AgentRuntime{}.runLocal(localOptimizeRequest).ok(),
                "local diagnostic mode must not pretend to complete an optimization workflow");
    requireTrue(!cc::ToolRegistry{}.interactiveToolSpecs().empty(),
                "interactive agent tools should be registered");
    auto auditCommand = cc::AgentCommandRouter{}.route("/audit");
    requireTrue(auditCommand.ok() && auditCommand.value().kind == cc::AgentCommandKind::RunAudit,
                "slash audit command should route to audit");
    auto naturalTask = cc::AgentCommandRouter{}.route("开始审计");
    requireTrue(naturalTask.ok() && naturalTask.value().kind == cc::AgentCommandKind::RunAgentTask,
                "natural language should be routed as agent task instead of keyword command");
    auto negativeOptimization = cc::AgentCommandRouter{}.route("不要优化项目");
    requireTrue(negativeOptimization.ok() &&
                    negativeOptimization.value().kind == cc::AgentCommandKind::RunAgentTask,
                "negative natural language must never trigger an implicit write flow");
    auto explicitOptimization = cc::AgentCommandRouter{}.route("/optimize 修订部署说明");
    requireTrue(explicitOptimization.ok() &&
                    explicitOptimization.value().kind ==
                        cc::AgentCommandKind::RunModePrefixedTask &&
                    explicitOptimization.value().context == "/optimize",
                "only the explicit optimize command should enter the repaired write flow");
    auto askMode = cc::AgentCommandRouter{}.route("/ask");
    requireTrue(askMode.ok() && askMode.value().kind == cc::AgentCommandKind::SetPermissionMode &&
                    askMode.value().prompt == "ask",
                "slash ask should switch permission mode");
    auto codeTask = cc::AgentCommandRouter{}.route("/code 修订README");
    requireTrue(codeTask.ok() &&
                    codeTask.value().kind == cc::AgentCommandKind::RunModePrefixedTask &&
                    codeTask.value().prompt == "修订README" && codeTask.value().context == "/code",
                "slash code with text should switch mode and route the task");
    auto bypassTask = cc::AgentCommandRouter{}.route("/bypass 检查原项目");
    requireTrue(bypassTask.ok() &&
                    bypassTask.value().kind == cc::AgentCommandKind::RunModePrefixedTask &&
                    bypassTask.value().context == "/bypass",
                "slash bypass with text should switch mode and route the task");
    auto unknownCommand = cc::AgentCommandRouter{}.route("/unknown");
    requireTrue(!unknownCommand.ok(), "unknown slash command should fail explicitly");
    const auto memoryRoot = std::filesystem::temp_directory_path() / "contest_memory_test";
    std::filesystem::remove_all(memoryRoot);
    std::filesystem::create_directories(memoryRoot);
    auto memory = cc::ProjectMemory{}.init(memoryRoot, cc::CompetitionType::BusinessInnovation);
    requireTrue(memory.ok(), "project memory should initialize");
    requireTrue(std::filesystem::exists(memoryRoot / ".project-trust" / "project_rules.json"),
                "project memory should write project_rules.json");

    const auto agentRoot = std::filesystem::temp_directory_path() / "contest_agent_runtime_test";
    std::filesystem::remove_all(agentRoot);
    std::filesystem::create_directories(agentRoot);
    requireTrue(cc::util::writeTextFile(agentRoot / "README.md", "#Title  \n\n\n正文  \n").ok(),
                "agent fixture markdown should be written");
    requireTrue(cc::util::writeTextFile(agentRoot / "main.cpp", "int main() { return 0; }\n").ok(),
                "agent fixture source should be written");
    const auto officeFile = contest_test::writeStoredZipFixture(
        agentRoot / "项目说明.docx",
        {{"word/document.xml", "<w:document><w:body><w:p><w:r><w:t>真实办公文件中的项目目标</w:t>"
                               "</w:r></w:p></w:body></w:document>"}});
    requireTrue(std::filesystem::is_regular_file(officeFile),
                "agent fixture office project file should be written");
    cc::AgentRunRequest request;
    request.userGoal = "请修一修 Markdown 文档";
    request.projectRoot = agentRoot;
    request.workspaceRoot = agentRoot / ".agent-workspace";
    request.allowWriteWorkspace = true;
    cc::AgentToolCall searchCall;
    searchCall.id = "search_1";
    searchCall.name = "search_project_text";
    searchCall.reason = "查找需要修订的正文";
    searchCall.input =
        cc::JsonValue::Object{{"query", "正文"}, {"max_files", 10}, {"max_matches", 5}};
    auto searched = cc::AgentRuntime{}.runTool(request, searchCall);
    requireTrue(searched.ok() && searched.value().ok, "search project text should execute");
    requireTrue(!searched.value().output.at("matches").asArray().empty(),
                "search project text should return matches");

    cc::AgentToolCall listCall;
    listCall.id = "list_1";
    listCall.name = "list_project_files";
    listCall.reason = "读取项目文件元数据";
    listCall.input = cc::JsonValue::Object{{"max_files", 10}};
    auto listed = cc::AgentRuntime{}.runTool(request, listCall);
    requireTrue(listed.ok() && listed.value().ok, "list project files should execute");
    requireTrue(listed.value().output.at("files").at(0).at("format").isString(),
                "file listing should include format metadata");

    cc::AgentToolCall inspectCall;
    inspectCall.id = "inspect_1";
    inspectCall.name = "inspect_project_file";
    inspectCall.reason = "确认代码文件读取策略";
    inspectCall.input = cc::JsonValue::Object{{"path", "main.cpp"}};
    auto inspected = cc::AgentRuntime{}.runTool(request, inspectCall);
    requireTrue(inspected.ok() && inspected.value().ok, "inspect project file should execute");
    requireTrue(inspected.value().output.at("suggested_tool").asString() == "read_text_file",
                "source files should be suggested for text reading");

    cc::AgentToolCall readCodeCall;
    readCodeCall.id = "read_code_1";
    readCodeCall.name = "read_text_file";
    readCodeCall.reason = "读取源码文件";
    readCodeCall.input = cc::JsonValue::Object{{"path", "main.cpp"}, {"max_bytes", 4000}};
    auto codeRead = cc::AgentRuntime{}.runTool(request, readCodeCall);
    requireTrue(codeRead.ok() && codeRead.value().ok, "source files should be readable");
    requireTrue(codeRead.value().output.at("content").asString().find("int main") !=
                    std::string::npos,
                "source reader should return code content");

    const auto deferredRoot =
        std::filesystem::temp_directory_path() / "contest_agent_deferred_runtime_test";
    std::filesystem::remove_all(deferredRoot);
    std::filesystem::create_directories(deferredRoot);
    const std::string deferredText =
        "deferred source remains readable on demand\n" + std::string(256U, 'x');
    requireTrue(cc::util::writeTextFile(deferredRoot / "large.notesblob", deferredText).ok(),
                "deferred text fixture should be written");
    requireTrue(cc::util::writeTextFile(deferredRoot / "large.bin", std::string(256U, '\0')).ok(),
                "deferred binary fixture should be written");
    requireTrue(
        cc::util::writeTextFile(deferredRoot / "auth.txt",
                                "api_key=sk-this-is-a-real-looking-test-secret-123456789\n" +
                                    std::string(256U, 's'))
            .ok(),
        "deferred sensitive fixture should be written");
    requireTrue(cc::util::writeTextFile(deferredRoot / "small.txt", "small\n").ok(),
                "copied text fixture should be written");

    cc::ImportLimits deferredLimits;
    deferredLimits.maxSingleFileBytes = 64U;
    deferredLimits.maxTotalBytes = 4096U;
    deferredLimits.maxExpandedBytes = 4096U;
    auto deferredContext = cc::ProjectLoader{deferredLimits}.load(deferredRoot);
    requireTrue(deferredContext.ok(), "directory import with deferred files should succeed");
    auto deferredInventory = cc::InventoryEngine{}.build(deferredContext.value());
    requireTrue(deferredInventory.ok(), "deferred files should enter the audit inventory");
    cc::AuditResult deferredAudit;
    deferredAudit.context = deferredContext.value();
    deferredAudit.inventory = deferredInventory.value();

    cc::AgentRunRequest deferredRequest;
    deferredRequest.userGoal = "inspect deferred inputs";
    deferredRequest.projectRoot = deferredAudit.context.inputRoot;
    deferredRequest.auditResult = &deferredAudit;

    cc::AgentToolCall deferredListCall;
    deferredListCall.id = "deferred_list";
    deferredListCall.name = "list_project_files";
    deferredListCall.reason = "list the complete audit inventory";
    deferredListCall.input = cc::JsonValue::Object{{"max_files", 20}};
    auto deferredListed = cc::AgentRuntime{}.runTool(deferredRequest, deferredListCall);
    requireTrue(deferredListed.ok() && deferredListed.value().ok,
                "agent should list files from the typed audit inventory");
    const auto& deferredFiles = deferredListed.value().output.at("files").asArray();
    const auto deferredListedAsset =
        std::find_if(deferredFiles.begin(), deferredFiles.end(), [](const cc::JsonValue& item) {
            return item.at("path").asString() == "large.notesblob";
        });
    requireTrue(
        deferredListedAsset != deferredFiles.end() &&
            deferredListedAsset->at("content_deferred").asBool(false) &&
            !deferredListedAsset->at("text_readable").asBool(true) &&
            deferredListedAsset->at("on_demand_text_candidate").asBool(false),
        "metadata-only files must remain visible with an explicit on-demand candidate flag");
    requireTrue(std::none_of(deferredFiles.begin(), deferredFiles.end(),
                             [](const cc::JsonValue& item) {
                                 return item.at("path").asString() == "auth.txt";
                             }) &&
                    deferredListed.value().output.at("omitted_sensitive_count").asNumber() >= 1.0,
                "sensitive deferred files must remain hidden from agent listings");

    auto cappedListCall = deferredListCall;
    cappedListCall.id = "deferred_list_capped";
    cappedListCall.input = cc::JsonValue::Object{{"max_files", 1}};
    auto cappedList = cc::AgentRuntime{}.runTool(deferredRequest, cappedListCall);
    requireTrue(cappedList.ok() && cappedList.value().ok &&
                    cappedList.value().output.at("files").asArray().size() == 1U &&
                    cappedList.value().output.at("truncated").asBool(false),
                "audit inventory listing must honor max_files");

    cc::AgentToolCall inspectDeferredCall;
    inspectDeferredCall.id = "inspect_deferred";
    inspectDeferredCall.name = "inspect_project_file";
    inspectDeferredCall.reason = "inspect metadata not copied into the workspace";
    inspectDeferredCall.input = cc::JsonValue::Object{{"path", "large.notesblob"}};
    auto inspectedDeferred = cc::AgentRuntime{}.runTool(deferredRequest, inspectDeferredCall);
    requireTrue(inspectedDeferred.ok() && inspectedDeferred.value().ok &&
                    inspectedDeferred.value().output.at("content_deferred").asBool(false) &&
                    inspectedDeferred.value().output.at("can_read_text").asBool(false),
                "inspect should return deferred metadata and detect bounded text availability");

    cc::AgentToolCall readDeferredCall;
    readDeferredCall.id = "read_deferred";
    readDeferredCall.name = "read_text_file";
    readDeferredCall.reason = "read a bounded prefix from the selected source directory";
    readDeferredCall.input = cc::JsonValue::Object{{"path", "large.notesblob"}, {"max_bytes", 48}};
    auto readDeferred = cc::AgentRuntime{}.runTool(deferredRequest, readDeferredCall);
    requireTrue(
        readDeferred.ok() && readDeferred.value().ok &&
            readDeferred.value().output.at("content_deferred").asBool(false) &&
            readDeferred.value().output.at("on_demand_original_read").asBool(false) &&
            readDeferred.value().output.at("truncated").asBool(false) &&
            readDeferred.value().output.at("content").asString().size() <= 48U &&
            readDeferred.value().output.at("content").asString().starts_with("deferred source"),
        "deferred directory text should be read from the original only within bounds");
    requireTrue(cc::util::readFileLimited(deferredRoot / "large.notesblob", deferredText.size()) ==
                    deferredText,
                "on-demand reads must not modify the original project");

    auto readBinaryCall = readDeferredCall;
    readBinaryCall.id = "read_deferred_binary";
    readBinaryCall.input = cc::JsonValue::Object{{"path", "large.bin"}, {"max_bytes", 48}};
    auto rejectedBinary = cc::AgentRuntime{}.runTool(deferredRequest, readBinaryCall);
    requireTrue(rejectedBinary.ok() && !rejectedBinary.value().ok &&
                    rejectedBinary.value().output.at("content").isNull(),
                "binary deferred content must not bypass read_text_file");

    auto inspectSecretCall = inspectDeferredCall;
    inspectSecretCall.id = "inspect_deferred_secret";
    inspectSecretCall.input = cc::JsonValue::Object{{"path", "auth.txt"}};
    auto rejectedSecret = cc::AgentRuntime{}.runTool(deferredRequest, inspectSecretCall);
    requireTrue(rejectedSecret.ok() && !rejectedSecret.value().ok &&
                    rejectedSecret.value().output.asObject().empty(),
                "inspect must not expose metadata for sensitive deferred assets");

    const auto singleDeferredPath =
        std::filesystem::temp_directory_path() / "contest_agent_single_deferred.notesblob";
    requireTrue(cc::util::writeTextFile(singleDeferredPath, deferredText).ok(),
                "single deferred text fixture should be written");
    auto singleDeferredContext = cc::ProjectLoader{deferredLimits}.load(singleDeferredPath);
    requireTrue(singleDeferredContext.ok() &&
                    singleDeferredContext.value().unpackStatus == "SINGLE_FILE_METADATA_ONLY",
                "oversized selected text file should import as metadata-only");
    auto singleDeferredInventory = cc::InventoryEngine{}.build(singleDeferredContext.value());
    requireTrue(singleDeferredInventory.ok(),
                "single metadata-only file should enter the audit inventory");
    cc::AuditResult singleDeferredAudit;
    singleDeferredAudit.context = singleDeferredContext.value();
    singleDeferredAudit.inventory = singleDeferredInventory.value();
    auto singleDeferredRequest = deferredRequest;
    singleDeferredRequest.projectRoot = singleDeferredAudit.context.inputRoot;
    singleDeferredRequest.auditResult = &singleDeferredAudit;
    auto singleList = cc::AgentRuntime{}.runTool(singleDeferredRequest, deferredListCall);
    requireTrue(singleList.ok() && singleList.value().ok &&
                    singleList.value()
                        .output.at("files")
                        .at(0)
                        .at("on_demand_text_candidate")
                        .asBool(false),
                "single metadata-only input should advertise bounded on-demand inspection");
    auto singleReadCall = readDeferredCall;
    singleReadCall.id = "read_single_deferred";
    singleReadCall.input = cc::JsonValue::Object{
        {"path", singleDeferredPath.filename().generic_string()}, {"max_bytes", 48}};
    auto singleRead = cc::AgentRuntime{}.runTool(singleDeferredRequest, singleReadCall);
    requireTrue(singleRead.ok() && singleRead.value().ok &&
                    singleRead.value().output.at("on_demand_original_read").asBool(false) &&
                    singleRead.value().output.at("content").asString().size() <= 48U,
                "a directly selected metadata-only text file should support a bounded read");

    std::error_code symlinkError;
    std::filesystem::rename(deferredRoot / "large.notesblob",
                            deferredRoot / "large-target.notesblob", symlinkError);
    if (!symlinkError) {
        std::filesystem::create_symlink("large-target.notesblob", deferredRoot / "large.notesblob",
                                        symlinkError);
    }
    if (!symlinkError) {
        auto rejectedSymlink = cc::AgentRuntime{}.runTool(deferredRequest, readDeferredCall);
        requireTrue(rejectedSymlink.ok() && !rejectedSymlink.value().ok,
                    "a deferred source that becomes a symlink must not be followed");
    }

    const auto deferredArchive = contest_test::writeStoredZipFixture(
        std::filesystem::temp_directory_path() / "contest_agent_deferred_archive.zip",
        {{"inside/large.txt", std::string(256U, 'a')}});
    auto archiveContext = cc::ProjectLoader{deferredLimits}.load(deferredArchive);
    requireTrue(archiveContext.ok(), "archive import with a deferred entry should succeed");
    auto archiveInventory = cc::InventoryEngine{}.build(archiveContext.value());
    requireTrue(archiveInventory.ok(), "deferred archive entry should enter the inventory");
    cc::AuditResult archiveAudit;
    archiveAudit.context = archiveContext.value();
    archiveAudit.inventory = archiveInventory.value();
    auto archiveRequest = deferredRequest;
    archiveRequest.projectRoot = archiveAudit.context.inputRoot;
    archiveRequest.auditResult = &archiveAudit;
    auto archiveList = cc::AgentRuntime{}.runTool(archiveRequest, deferredListCall);
    requireTrue(
        archiveList.ok() && archiveList.value().ok &&
            archiveList.value().output.at("files").at(0).at("path").asString() ==
                "inside/large.txt" &&
            archiveList.value().output.at("files").at(0).at("content_deferred").asBool(false) &&
            !archiveList.value()
                 .output.at("files")
                 .at(0)
                 .at("on_demand_text_candidate")
                 .asBool(true),
        "archive-internal deferred entries should be listed without a read-through hint");
    auto inspectArchiveDeferred = inspectDeferredCall;
    inspectArchiveDeferred.id = "inspect_archive_deferred";
    inspectArchiveDeferred.input = cc::JsonValue::Object{{"path", "inside/large.txt"}};
    auto archiveMetadata = cc::AgentRuntime{}.runTool(archiveRequest, inspectArchiveDeferred);
    requireTrue(archiveMetadata.ok() && archiveMetadata.value().ok &&
                    archiveMetadata.value().output.at("content_deferred").asBool(false) &&
                    !archiveMetadata.value().output.at("can_read_text").asBool(true),
                "archive-internal deferred entries should expose metadata but no direct read path");
    auto readArchiveDeferred = readDeferredCall;
    readArchiveDeferred.id = "read_archive_deferred";
    readArchiveDeferred.input =
        cc::JsonValue::Object{{"path", "inside/large.txt"}, {"max_bytes", 48}};
    auto rejectedArchiveRead = cc::AgentRuntime{}.runTool(archiveRequest, readArchiveDeferred);
    requireTrue(rejectedArchiveRead.ok() && !rejectedArchiveRead.value().ok &&
                    rejectedArchiveRead.value().summary.find("归档内部") != std::string::npos,
                "archive-internal deferred content must not bypass extraction policy");

    cc::AgentToolCall writeCall;
    writeCall.id = "write_1";
    writeCall.name = "write_workspace_file";
    writeCall.reason = "写入新的工作区说明";
    writeCall.input =
        cc::JsonValue::Object{{"path", "notes/TODO.md"}, {"content", "# TODO\n补齐证明材料\n"}};
    auto readOnlyRequest = request;
    readOnlyRequest.allowWriteWorkspace = false;
    auto deniedArtifact = cc::AgentRuntime{}.runTool(readOnlyRequest, writeCall);
    requireTrue(deniedArtifact.ok() && !deniedArtifact.value().ok,
                "Ask and Plan requests must not inherit workspace write access");
    auto writtenArtifact = cc::AgentRuntime{}.runTool(request, writeCall);
    requireTrue(writtenArtifact.ok() && writtenArtifact.value().ok,
                "workspace text writer should execute");
    requireTrue(std::filesystem::exists(agentRoot / ".agent-workspace" / "notes" / "TODO.md"),
                "workspace text writer should create a workspace file");

    cc::AgentRunRequest auditRequest;
    auditRequest.userGoal = "审查当前项目";
    auditRequest.projectRoot = agentRoot;
    auditRequest.auditOptions.rulesDir = sourceDir() / "rules";
    auditRequest.requireAudit = true;
    auto blockedBeforeAudit = cc::AgentRuntime{}.runTool(auditRequest, listCall);
    requireTrue(blockedBeforeAudit.ok() && !blockedBeforeAudit.value().ok,
                "project read tools should wait until the audit creates an isolated copy");
    cc::AgentToolCall auditToolCall;
    auditToolCall.id = "audit_1";
    auditToolCall.name = "run_project_audit";
    auditToolCall.reason = "先取得确定性规则和证据结果";
    auditToolCall.input = cc::JsonValue::Object{};
    auto auditExecution = cc::AgentRuntime{}.runToolExecution(auditRequest, auditToolCall);
    requireTrue(auditExecution.ok(), "agent audit tool should execute");
    requireTrue(auditExecution.value().auditResult.has_value(),
                "agent audit tool should return a typed audit result");
    requireTrue(auditExecution.value().observations.size() ==
                    cc::StagedAuditPipeline::stages().size() + 1U,
                "agent audit tool should expose every deterministic stage and its summary");
    requireTrue(auditExecution.value().observations.back().toolName == "run_project_audit",
                "agent audit tool should finish with an aggregate observation for the brain");

    auto auditedRequest = auditRequest;
    auditedRequest.requireAudit = false;
    auditedRequest.auditResult = &(*auditExecution.value().auditResult);
    auditedRequest.projectRoot = auditedRequest.auditResult->context.inputRoot;
    auditedRequest.workspaceRoot = auditedRequest.auditResult->context.workspaceRoot / "agent";
    auto inspectOfficeCall = inspectCall;
    inspectOfficeCall.id = "inspect_office_1";
    inspectOfficeCall.input = cc::JsonValue::Object{{"path", "项目说明.docx"}};
    auto inspectedOffice = cc::AgentRuntime{}.runTool(auditedRequest, inspectOfficeCall);
    requireTrue(inspectedOffice.ok() && inspectedOffice.value().ok &&
                    inspectedOffice.value().output.at("suggested_tool").asString() ==
                        "read_extracted_document" &&
                    inspectedOffice.value().output.at("can_read_extracted_document").asBool(false),
                "office project files should route to the extracted document reader");
    cc::AgentToolCall readOfficeCall;
    readOfficeCall.id = "read_office_1";
    readOfficeCall.name = "read_extracted_document";
    readOfficeCall.reason = "读取真实 DOCX 项目文件";
    readOfficeCall.input = cc::JsonValue::Object{{"path", "项目说明.docx"}, {"max_bytes", 8000}};
    auto officeRead = cc::AgentRuntime{}.runTool(auditedRequest, readOfficeCall);
    requireTrue(officeRead.ok() && officeRead.value().ok &&
                    officeRead.value().output.at("extracted_from_project_file").asBool(false) &&
                    officeRead.value().output.at("content").asString().find("真实办公文件") !=
                        std::string::npos,
                "agent should read content extracted from the real DOCX project file");

    auto run = cc::AgentRuntime{}.runLocal(request);
    requireTrue(run.ok(), "local agent runtime should execute");
    requireTrue(run.value().observations.size() >= 2U,
                "local agent should inspect project context");
    requireTrue(run.value().events.size() >= run.value().observations.size(),
                "agent run should expose structured turn events");
    requireTrue(run.value().trace.at("events").isArray(), "agent trace should include events");
    cc::setAgentFinalAnswer(run.value(), "Brain final", "Brain final answer");
    requireTrue(run.value().finalAnswer == "Brain final", "final answer should be replaceable");
    requireTrue(run.value().trace.at("final_answer").asString() == "Brain final",
                "trace should reflect replaced final answer");

    cc::AuditSession session;
    session.sessionId = "session-test";
    session.toolOutputs = {"inventory_project"};
    session.result.toolOutputs = session.toolOutputs;
    const auto sessionPath = std::filesystem::temp_directory_path() / "contest_session.json";
    requireTrue(cc::AuditSessionStore{}.save(session, sessionPath).ok(),
                "session store should save AuditSession");
}
