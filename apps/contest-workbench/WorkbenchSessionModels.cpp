/**
 * @file WorkbenchSessionModels.cpp
 * @brief 将审计结果和 agentic runtime 信息转换为会话工作区模型。
 */

#include "WorkbenchSessionModels.hpp"

#include "AuditResultModels.hpp"
#include "cc/agent/PermissionGate.hpp"
#include "cc/agent/ToolRegistry.hpp"

#include <QStringList>

#include <algorithm>

namespace workbench {
namespace {

[[nodiscard]] QString pathText(const std::filesystem::path& value) {
    return QString::fromStdString(value.generic_string());
}

[[nodiscard]] QString stringText(const std::string& value) {
    return QString::fromStdString(value);
}

[[nodiscard]] QString joinedStrings(const std::vector<std::string>& values) {
    QStringList items;
    for (const auto& value : values) {
        items.push_back(stringText(value));
    }
    return items.join("、");
}

[[nodiscard]] QString inferKind(const QString& role) {
    const auto key = role.trimmed().toLower();
    if (key == "用户" || key == "user") {
        return "user";
    }
    if (key == "计划" || key == "plan") {
        return "plan";
    }
    if (key == "工具" || key == "tool") {
        return "tool";
    }
    if (key == "产物" || key == "artifact") {
        return "artifact";
    }
    if (key == "系统" || key == "system") {
        return "system";
    }
    return "assistant";
}

[[nodiscard]] QVariantMap messageItem(const QString& role, const QString& text,
                                      const QString& context) {
    QVariantMap item;
    item["role"] = role;
    item["text"] = text;
    item["context"] = context;
    item["kind"] = inferKind(role);
    item["detail"] = QString{};
    item["ok"] = true;
    return item;
}

[[nodiscard]] QString toolDisplayName(const std::string& name) {
    if (name == "run_project_audit") {
        return "运行规则审计";
    }
    if (name == "summarize_audit_session") {
        return "压缩审计上下文";
    }
    if (name == "list_project_files") {
        return "翻阅项目文件";
    }
    if (name == "inspect_project_file") {
        return "检查文件格式";
    }
    if (name == "read_text_file") {
        return "读取文本文件";
    }
    if (name == "inspect_archive") {
        return "检查材料包";
    }
    if (name == "search_project_text") {
        return "搜索项目文本";
    }
    if (name == "draft_markdown_revision") {
        return "修订 Markdown";
    }
    if (name == "write_workspace_file") {
        return "写入工作区文件";
    }
    if (name == "inventory_project") {
        return "整理材料";
    }
    if (name == "extract_text") {
        return "读取文本";
    }
    if (name == "detect_competition_type") {
        return "判断赛道";
    }
    if (name == "build_cpir") {
        return "生成项目画像";
    }
    if (name == "extract_claims") {
        return "提取关键声明";
    }
    if (name == "match_evidence") {
        return "匹配证据";
    }
    if (name == "check_consistency") {
        return "检查一致性";
    }
    if (name == "run_rules") {
        return "执行规则";
    }
    if (name == "calculate_trust_score") {
        return "计算评分";
    }
    if (name == "generate_fix_tasks") {
        return "生成补证任务";
    }
    if (name == "generate_repair_plan") {
        return "整理修复计划";
    }
    if (name == "export_markdown_report") {
        return "导出 Markdown";
    }
    if (name == "export_json_report") {
        return "导出 JSON";
    }
    if (name == "verify_diff") {
        return "比较两次审计";
    }
    if (name == "explain_audit_finding") {
        return "解释风险";
    }
    if (name == "generate_defense_questions") {
        return "生成答辩问题";
    }
    return stringText(name);
}

[[nodiscard]] const std::vector<std::string>& auditToolFlow() {
    static const std::vector<std::string> flow = {
        "inventory_project",     "extract_text",       "detect_competition_type", "build_cpir",
        "extract_claims",        "match_evidence",     "check_consistency",       "run_rules",
        "calculate_trust_score", "generate_fix_tasks", "generate_repair_plan"};
    return flow;
}

[[nodiscard]] int flowIndex(const std::string& name) {
    const auto& flow = auditToolFlow();
    const auto iter = std::find(flow.begin(), flow.end(), name);
    return iter == flow.end() ? -1 : static_cast<int>(std::distance(flow.begin(), iter));
}

[[nodiscard]] QVariantMap artifactItem(const QString& pageKey, const QString& title,
                                       const QString& kind, const QString& detail,
                                       bool available) {
    QVariantMap item;
    item["pageKey"] = pageKey;
    item["title"] = title;
    item["kind"] = kind;
    item["detail"] = detail;
    item["available"] = available;
    return item;
}

[[nodiscard]] QString importStatusText(const std::string& status) {
    if (status == "SINGLE_FILE_COPIED_TO_WORKSPACE") {
        return "单份材料已建立安全副本";
    }
    if (status == "SINGLE_FILE_METADATA_ONLY") {
        return "大型文件已识别，内容按需读取";
    }
    if (status == "ARCHIVE_METADATA_ONLY") {
        return "归档已识别，内容暂按元数据接入";
    }
    if (status == "INPUT_METADATA_ONLY") {
        return "特殊输入已识别，未打开其内容";
    }
    if (status == "DIRECTORY_COPIED_TO_WORKSPACE") {
        return "已建立安全工作副本";
    }
    if (status == "ZIP_EXTRACTED") {
        return "压缩包已安全解包";
    }
    if (status == "ARCHIVE_EXTRACTED") {
        return "材料包已安全解包";
    }
    if (status.empty()) {
        return "等待审计";
    }
    return stringText(status);
}

[[nodiscard]] QString permissionDetail(cc::ToolPermission permission, bool allowed,
                                       const QString& accessMode) {
    if (accessMode == "plan") {
        if (permission == cc::ToolPermission::ModifyOriginalProject ||
            permission == cc::ToolPermission::ExecuteCommand ||
            permission == cc::ToolPermission::NetworkAccess ||
            permission == cc::ToolPermission::LLMAccess) {
            return "计划模式下不会执行此类动作";
        }
    }
    switch (permission) {
    case cc::ToolPermission::ReadProjectFiles:
        return accessMode == "bypass" ? "Bypass 模式下经授权读取原项目路径"
                                      : "只读取隔离工作区中的项目副本";
    case cc::ToolPermission::ReadExternalFiles:
        return allowed ? "Bypass 模式下允许读取用户授权的项目外文件" : "默认拒绝读取项目外文件";
    case cc::ToolPermission::WriteWorkspace:
        return allowed ? "Code/扩展读取模式可写入隔离会话工作区"
                       : "Ask/Plan 模式不写入会话工作区";
    case cc::ToolPermission::ModifyOriginalProject:
        return "所有模式都禁止覆盖原始项目";
    case cc::ToolPermission::ExecuteCommand:
        return "所有模式都不提供自由脚本或 shell 执行";
    case cc::ToolPermission::NetworkAccess:
        return allowed ? "已明确授权联网；请求受 HTTPS、时限和大小边界约束" : "默认关闭联网";
    case cc::ToolPermission::LLMAccess:
        return allowed ? "已明确授权调用 LLM；最终评分仍由规则引擎裁决"
                       : "默认关闭 LLM 调用";
    case cc::ToolPermission::ExportReport:
        return "允许导出用户指定的 Markdown/JSON 报告";
    }
    return "未知权限";
}

[[nodiscard]] QString permissionName(cc::ToolPermission permission) {
    switch (permission) {
    case cc::ToolPermission::ReadProjectFiles:
        return "读取项目副本";
    case cc::ToolPermission::ReadExternalFiles:
        return "读取项目外文件";
    case cc::ToolPermission::WriteWorkspace:
        return "写入会话工作区";
    case cc::ToolPermission::ModifyOriginalProject:
        return "修改原始项目";
    case cc::ToolPermission::ExecuteCommand:
        return "执行外部命令";
    case cc::ToolPermission::NetworkAccess:
        return "联网访问";
    case cc::ToolPermission::LLMAccess:
        return "调用大模型";
    case cc::ToolPermission::ExportReport:
        return "导出报告";
    }
    return "未知权限";
}

} // namespace

QVariantMap projectContext(const cc::AuditResult* result,
                           const QString& normalizedProjectPath) {
    QVariantMap item;
    if (result == nullptr) {
        item["projectName"] = "等待导入";
        item["originalRoot"] = normalizedProjectPath;
        item["inputRoot"] = "";
        item["workspaceRoot"] = ".workspaces/<session_id>";
        item["sessionId"] = "未创建";
        item["unpackStatus"] = "等待审计";
        item["unpackStatusCode"] = "WAITING";
        item["archiveInput"] = false;
        item["inputFileCount"] = 0;
        item["warnings"] = "";
        return item;
    }

    const auto& context = result->context;
    item["projectName"] = stringText(context.projectName);
    item["originalRoot"] = pathText(context.originalRoot);
    item["inputRoot"] = pathText(context.inputRoot);
    item["workspaceRoot"] = pathText(context.workspaceRoot);
    item["sessionId"] = stringText(context.sessionId);
    item["unpackStatus"] = importStatusText(context.unpackStatus);
    item["unpackStatusCode"] = stringText(context.unpackStatus);
    item["archiveInput"] = context.archiveInput;
    item["inputFileCount"] = static_cast<int>(context.inputFiles.size());
    item["warnings"] = joinedStrings(context.warnings);
    return item;
}

QVariantList sessionHistory(const cc::AuditResult* result,
                            const std::vector<SessionMessage>& conversation,
                            const QString& normalizedProjectPath) {
    QVariantList items;

    const auto appendMessage = [&](const SessionMessage& message) {
        if (message.text.trimmed().isEmpty() && message.detail.trimmed().isEmpty()) {
            return;
        }
        QVariantMap item = messageItem(message.role, message.text, message.context);
        if (!message.kind.isEmpty()) {
            item["kind"] = message.kind;
        }
        item["detail"] = message.detail;
        item["target"] = message.target;
        item["ok"] = message.ok;
        items.push_back(item);
    };

    for (const auto& message : conversation) {
        appendMessage(message);
    }
    if (!items.isEmpty()) {
        return items;
    }

    if (!normalizedProjectPath.isEmpty()) {
        items.push_back(messageItem(
            "系统", QStringLiteral("当前材料包：%1").arg(normalizedProjectPath), "材料已选择"));
    }
    if (result != nullptr) {
        items.push_back(messageItem("工具",
                                    QStringLiteral("审计完成：评分 %1/100，必须处理 %2 "
                                                   "个，需要关注 %3 个，补证任务 %4 个。")
                                        .arg(result->trustScore.totalScore)
                                        .arg(blockerCount(*result))
                                        .arg(warningCount(*result))
                                        .arg(result->fixTasks.size()),
                                    "审计完成"));
        items.push_back(messageItem("智能体", agentSummary(result), "下一步建议"));
    }
    return items;
}

QVariantList toolCards(const cc::AuditResult* result,
                       const std::optional<cc::AuditDiff>& auditDiff, bool agentRunning,
                       int activeStep, int completedSteps) {
    (void)auditDiff;
    QVariantList items;
    const auto& names = auditToolFlow();
    for (const auto& name : names) {
        QVariantMap item;
        item["name"] = toolDisplayName(name);
        item["technicalName"] = stringText(name);
        item["status"] = result != nullptr ? "完成" : "等待";
        item["detail"] = "运行审计后自动完成";

        const auto index = flowIndex(name);
        if (agentRunning && index >= 0) {
            if (index < completedSteps) {
                item["status"] = "完成";
                item["detail"] = "已完成，结果会进入当前会话";
            } else if (index == activeStep) {
                item["status"] = "进行中";
                item["detail"] = "智能体正在调用此受控步骤";
            } else {
                item["status"] = "等待";
                item["detail"] = "等待前置步骤完成";
            }
            items.push_back(item);
            continue;
        }

        if (result != nullptr) {
            if (name == "summarize_audit_session") {
                item["status"] = "可追问";
                item["detail"] = "把评分、风险和补证任务压缩为对话上下文";
            } else if (name == "list_project_files") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可在权限内枚举项目副本文件";
            } else if (name == "inspect_project_file") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可判断文件格式、语言和下一步读取策略";
            } else if (name == "read_text_file") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可读取项目内文本或 Markdown 片段";
            } else if (name == "inspect_archive") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可安全列出 zip/tar/7z/tgz 包条目";
            } else if (name == "search_project_text") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可在项目副本文本中按关键词搜索";
            } else if (name == "draft_markdown_revision") {
                item["status"] = "可追问";
                item["detail"] = "只写入工作区修订稿，不覆盖原项目";
            } else if (name == "write_workspace_file") {
                item["status"] = "可追问";
                item["detail"] = "Brain 可把代码、配置、清单和新文档写入会话工作区";
            } else if (name == "inventory_project") {
                item["detail"] = QStringLiteral("资产 %1 个，输入文件 %2 个")
                                     .arg(result->inventory.assets.size())
                                     .arg(result->context.inputFiles.size());
            } else if (name == "extract_text") {
                item["detail"] = QStringLiteral("可审计文本 %1 份").arg(result->corpus.size());
            } else if (name == "detect_competition_type") {
                item["detail"] = QStringLiteral("赛道 %1，置信度 %2")
                                     .arg(stringText(cc::toString(result->cpir.competitionType)))
                                     .arg(result->cpir.competitionConfidence);
            } else if (name == "build_cpir") {
                item["detail"] = "已生成项目画像和关键上下文";
            } else if (name == "extract_claims") {
                item["detail"] = QStringLiteral("声明 %1 条").arg(result->claims.size());
            } else if (name == "match_evidence") {
                item["detail"] =
                    QStringLiteral("证据匹配 %1 条").arg(result->evidenceMatches.size());
            } else if (name == "check_consistency") {
                item["detail"] =
                    QStringLiteral("一致性问题 %1 个").arg(result->consistencyIssues.size());
            } else if (name == "run_rules") {
                item["detail"] = QStringLiteral("规则风险 %1 个").arg(result->findings.size());
            } else if (name == "calculate_trust_score") {
                item["detail"] = QStringLiteral("可信评分 %1，可信债务 %2")
                                     .arg(result->trustScore.totalScore)
                                     .arg(result->trustScore.trustDebt);
            } else if (name == "generate_fix_tasks") {
                item["detail"] = QStringLiteral("补证任务 %1 个").arg(result->fixTasks.size());
            } else if (name == "generate_repair_plan") {
                item["detail"] = "已生成修复建议，不覆盖原项目";
            }
        }
        items.push_back(item);
    }
    return items;
}

QVariantList permissionCards(bool llmApproved, const QString& accessMode) {
    QVariantList items;
    cc::PermissionGate gate;
    for (const auto permission : gate.permissions()) {
        bool allowed = gate.isAllowed(permission);
        if (permission == cc::ToolPermission::ReadExternalFiles) {
            allowed = accessMode == "bypass";
        } else if (permission == cc::ToolPermission::WriteWorkspace) {
            allowed = accessMode == "code" || accessMode == "bypass";
        } else if (permission == cc::ToolPermission::NetworkAccess ||
                   permission == cc::ToolPermission::LLMAccess) {
            allowed = llmApproved && accessMode != "plan";
        } else if (permission == cc::ToolPermission::ModifyOriginalProject ||
                   permission == cc::ToolPermission::ExecuteCommand) {
            allowed = false;
        }
        QVariantMap item;
        item["name"] = permissionName(permission);
        item["technicalName"] = stringText(cc::toString(permission));
        item["status"] = accessMode == "plan" && !allowed ? "计划阻断"
                                                           : allowed ? "允许" : "默认拒绝";
        item["allowed"] = allowed;
        item["detail"] = permissionDetail(permission, allowed, accessMode);
        items.push_back(item);
    }
    return items;
}

QVariantList artifacts(const cc::AuditResult* result,
                       const std::optional<cc::AuditDiff>& auditDiff, const QString& agentResult) {
    QVariantList items;
    if (result == nullptr) {
        const auto waiting = QStringLiteral("完成项目审计后可查看");
        items.push_back(artifactItem("dashboard", "可信评分总览", "总览", waiting, false));
        items.push_back(artifactItem("assets", "材料资产清单", "数据", waiting, false));
        items.push_back(artifactItem("cpir", "项目画像", "画像", waiting, false));
        items.push_back(artifactItem("claims", "声明与证据", "证据", waiting, false));
        items.push_back(artifactItem("consistency", "材料一致性", "校验", waiting, false));
        items.push_back(artifactItem("findings", "规则风险", "风险", waiting, false));
        items.push_back(artifactItem("tasks", "补证与修复任务", "计划", waiting, false));
        items.push_back(
            artifactItem("diff", "二次审计差分", "对比", "选择两份审计数据包进行比较", true));
        items.push_back(artifactItem("brain", "智能体运行记录", "记录",
                                     "配置模型、运行混合研判并查看工具轨迹", true));
        items.push_back(artifactItem("report", "审计报告导出", "导出", waiting, false));
        return items;
    }

    items.push_back(artifactItem(
        "dashboard", "可信评分总览", "总览",
        QStringLiteral("评分 %1 · 必须处理 %2 · 需要关注 %3")
            .arg(result->trustScore.totalScore)
            .arg(blockerCount(*result))
            .arg(warningCount(*result)),
        true));
    items.push_back(artifactItem(
        "assets", "材料资产清单", "数据",
        QStringLiteral("已识别 %1 份文件").arg(result->inventory.assets.size()), true));
    items.push_back(artifactItem("cpir", "项目画像", "画像",
                                 QStringLiteral("%1 · 置信度 %2")
                                     .arg(stringText(cc::toString(result->cpir.competitionType)))
                                     .arg(result->cpir.competitionConfidence),
                                 true));
    items.push_back(artifactItem(
        "claims", "声明与证据", "证据",
        QStringLiteral("声明 %1 条 · 证据匹配 %2 条")
            .arg(result->claims.size())
            .arg(result->evidenceMatches.size()),
        true));
    items.push_back(artifactItem(
        "consistency", "材料一致性", "校验",
        QStringLiteral("发现 %1 个跨材料一致性问题").arg(result->consistencyIssues.size()), true));
    items.push_back(artifactItem(
        "findings", "规则风险", "风险",
        QStringLiteral("规则命中 %1 项").arg(result->findings.size()), true));
    items.push_back(artifactItem(
        "tasks", "补证与修复任务", "计划",
        QStringLiteral("按优先级整理 %1 个任务").arg(result->fixTasks.size()), true));
    items.push_back(artifactItem(
        "diff", "实际变更与二次审计", "对比",
        !result->repairPlan.diffText.empty()
            ? QStringLiteral("已绑定真实 changes.patch · %1 字节")
                  .arg(result->repairPlan.diffText.size())
            : auditDiff.has_value() ? stringText(auditDiff->summary)
                                    : "选择两份审计数据包进行比较",
        true));
    items.push_back(artifactItem(
        "brain", "智能体运行记录", "记录",
        agentResult.isEmpty() ? "可运行混合研判并查看工具轨迹" : "已生成智能体运行结果和工具轨迹",
        true));
    items.push_back(
        artifactItem("report", "审计报告导出", "导出", "导出 Markdown 报告或 JSON 审计数据包", true));
    return items;
}

QString agentSummary(const cc::AuditResult* result) {
    if (result == nullptr) {
        return "请选择项目材料包并开始审计。";
    }

    QString summary = QStringLiteral("当前可信评分 %1/100，待补强空间 %2 分。")
                          .arg(result->trustScore.totalScore)
                          .arg(result->trustScore.trustDebt);
    const auto blocker = std::find_if(
        result->findings.begin(), result->findings.end(),
        [](const cc::AuditFinding& finding) { return finding.severity == cc::Severity::Blocker; });
    if (blocker != result->findings.end()) {
        summary += QStringLiteral(" 优先处理必须项：%1。").arg(stringText(blocker->reason));
    }
    if (!result->fixTasks.empty()) {
        const auto& task = result->fixTasks.front();
        summary += QStringLiteral(" 下一步补证：%1。").arg(stringText(task.title));
    }
    summary += " 所有建议均来自规则结果、证据状态和补证任务，不生成虚假材料。";
    return summary;
}

} // namespace workbench
