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
        return "读取材料内容";
    }
    if (name == "detect_competition_type") {
        return "判断项目类型";
    }
    if (name == "build_cpir") {
        return "整理项目信息";
    }
    if (name == "extract_claims") {
        return "找出需要举证的成果";
    }
    if (name == "match_evidence") {
        return "核对证明材料";
    }
    if (name == "check_consistency") {
        return "核对材料表述";
    }
    if (name == "run_rules") {
        return "查找提交问题";
    }
    if (name == "calculate_trust_score") {
        return "计算评分";
    }
    if (name == "generate_fix_tasks") {
        return "整理修改清单";
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
                                       const QString& kind, const QString& detail, bool available) {
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
        return allowed ? "Code/扩展读取模式可写入隔离会话工作区" : "Ask/Plan 模式不写入会话工作区";
    case cc::ToolPermission::ModifyOriginalProject:
        return "所有模式都禁止覆盖原始项目";
    case cc::ToolPermission::ExecuteCommand:
        return "所有模式都不提供自由脚本或 shell 执行";
    case cc::ToolPermission::NetworkAccess:
        return allowed ? "有效 LLM 配置已启用联网；请求受 HTTPS、时限和大小边界约束"
                       : "未配置有效 LLM 服务，不会联网";
    case cc::ToolPermission::LLMAccess:
        return allowed ? "有效配置已启用 LLM；最终评分仍由规则引擎裁决"
                       : "未配置有效 LLM 服务，不会调用模型";
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

QVariantMap projectContext(const cc::AuditResult* result, const QString& normalizedProjectPath) {
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
                                    QStringLiteral("材料已经看完：%1 分，%2 个问题要先处理，"
                                                   "另有 %3 个地方建议补齐。")
                                        .arg(result->trustScore.totalScore)
                                        .arg(blockerCount(*result))
                                        .arg(warningCount(*result)),
                                    "检查完成"));
        items.push_back(messageItem("智能体", agentSummary(result), "下一步建议"));
    }
    return items;
}

QVariantList toolCards(const cc::AuditResult* result, const std::optional<cc::AuditDiff>& auditDiff,
                       bool agentRunning, int activeStep, int completedSteps) {
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
                item["detail"] =
                    QStringLiteral("判断为 %1，把握 %2%")
                        .arg(stringText(cc::toString(result->cpir.competitionType)))
                        .arg(QString::number(result->cpir.competitionConfidence * 100.0, 'f', 0));
            } else if (name == "build_cpir") {
                item["detail"] = "已整理项目介绍里的关键信息";
            } else if (name == "extract_claims") {
                item["detail"] =
                    QStringLiteral("找到 %1 条需要证明的成果").arg(result->claims.size());
            } else if (name == "match_evidence") {
                item["detail"] =
                    QStringLiteral("完成 %1 条证明材料核对").arg(result->evidenceMatches.size());
            } else if (name == "check_consistency") {
                item["detail"] = QStringLiteral("发现 %1 处材料表述需要对齐")
                                     .arg(result->consistencyIssues.size());
            } else if (name == "run_rules") {
                item["detail"] =
                    QStringLiteral("发现 %1 个需要查看的问题").arg(result->findings.size());
            } else if (name == "calculate_trust_score") {
                item["detail"] = QStringLiteral("当前 %1 分，还有 %2 分可以通过完善材料恢复")
                                     .arg(result->trustScore.totalScore)
                                     .arg(result->trustScore.trustDebt);
            } else if (name == "generate_fix_tasks") {
                item["detail"] =
                    QStringLiteral("整理出 %1 项修改建议").arg(result->fixTasks.size());
            } else if (name == "generate_repair_plan") {
                item["detail"] = "修改建议已经整理好，原项目不会被改动";
            }
        }
        items.push_back(item);
    }
    return items;
}

QVariantList permissionCards(bool llmConfigured, const QString& accessMode) {
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
            allowed = llmConfigured && accessMode != "plan";
        } else if (permission == cc::ToolPermission::ModifyOriginalProject ||
                   permission == cc::ToolPermission::ExecuteCommand) {
            allowed = false;
        }
        QVariantMap item;
        item["name"] = permissionName(permission);
        item["technicalName"] = stringText(cc::toString(permission));
        item["status"] = accessMode == "plan" && !allowed ? "计划阻断"
                         : allowed                        ? "允许"
                                                          : "默认拒绝";
        item["allowed"] = allowed;
        item["detail"] = permissionDetail(permission, allowed, accessMode);
        items.push_back(item);
    }
    return items;
}

QVariantList artifacts(const cc::AuditResult* result, const std::optional<cc::AuditDiff>& auditDiff,
                       const QString& agentResult) {
    QVariantList items;
    if (result == nullptr) {
        const auto waiting = QStringLiteral("完成项目审计后可查看");
        items.push_back(artifactItem("dashboard", "检查结果总览", "总览", waiting, false));
        items.push_back(artifactItem("assets", "材料资产清单", "数据", waiting, false));
        items.push_back(artifactItem("cpir", "项目基本信息", "信息", waiting, false));
        items.push_back(artifactItem("claims", "成果与证明材料", "证明", waiting, false));
        items.push_back(artifactItem("consistency", "材料内容是否矛盾", "核对", waiting, false));
        items.push_back(artifactItem("findings", "发现的问题", "问题", waiting, false));
        items.push_back(artifactItem("tasks", "下一步修改清单", "待办", waiting, false));
        items.push_back(
            artifactItem("diff", "修改前后对比", "对比", "导入修改后的材料即可重新检查", true));
        items.push_back(artifactItem("brain", "智能辅助检查", "辅助",
                                     "可选的联网辅助功能，不影响本地规则检查", true));
        items.push_back(artifactItem("report", "下载检查报告", "导出", waiting, false));
        return items;
    }

    items.push_back(artifactItem("dashboard", "检查结果总览", "总览",
                                 QStringLiteral("%1 分，%2 个问题要处理，%3 个地方建议补齐")
                                     .arg(result->trustScore.totalScore)
                                     .arg(blockerCount(*result))
                                     .arg(warningCount(*result)),
                                 true));
    items.push_back(artifactItem(
        "assets", "材料资产清单", "数据",
        QStringLiteral("已识别 %1 份文件").arg(result->inventory.assets.size()), true));
    items.push_back(
        artifactItem("cpir", "项目基本信息", "信息",
                     QStringLiteral("%1，判断把握 %2%")
                         .arg(stringText(cc::toString(result->cpir.competitionType)))
                         .arg(QString::number(result->cpir.competitionConfidence * 100.0, 'f', 0)),
                     true));
    items.push_back(artifactItem("claims", "成果与证明材料", "证明",
                                 QStringLiteral("识别 %1 条成果，已完成 %2 条证明材料核对")
                                     .arg(result->claims.size())
                                     .arg(result->evidenceMatches.size()),
                                 true));
    items.push_back(artifactItem(
        "consistency", "材料内容是否矛盾", "核对",
        QStringLiteral("发现 %1 个跨材料一致性问题").arg(result->consistencyIssues.size()), true));
    items.push_back(artifactItem("findings", "发现的问题", "问题",
                                 QStringLiteral("规则命中 %1 项").arg(result->findings.size()),
                                 true));
    items.push_back(
        artifactItem("tasks", "下一步修改清单", "待办",
                     QStringLiteral("按优先级整理 %1 个任务").arg(result->fixTasks.size()), true));
    items.push_back(artifactItem("diff", "修改前后对比", "对比",
                                 !result->repairPlan.diffText.empty()
                                     ? QStringLiteral("已生成修改建议，涉及 %1 字节的文本变更")
                                           .arg(result->repairPlan.diffText.size())
                                 : auditDiff.has_value() ? stringText(auditDiff->summary)
                                                         : "选择两份审计数据包进行比较",
                                 true));
    items.push_back(artifactItem(
        "brain", "智能辅助检查", "辅助",
        agentResult.isEmpty() ? "可选的联网辅助检查" : "已生成智能辅助检查结果", true));
    items.push_back(artifactItem("report", "下载检查报告", "导出", "下载便于阅读的报告", true));
    return items;
}

QString agentSummary(const cc::AuditResult* result) {
    if (result == nullptr) {
        return "请选择项目材料包并开始审计。";
    }

    QString summary = QStringLiteral("目前是 %1 分。").arg(result->trustScore.totalScore);
    const auto blocker = std::find_if(
        result->findings.begin(), result->findings.end(),
        [](const cc::AuditFinding& finding) { return finding.severity == cc::Severity::Blocker; });
    if (blocker != result->findings.end()) {
        summary += QStringLiteral(" 最先要处理的是：%1").arg(stringText(blocker->reason));
    }
    if (!result->fixTasks.empty()) {
        const auto& task = result->fixTasks.front();
        summary += QStringLiteral(" 接着可以做：%1。").arg(stringText(task.title));
    }
    return summary;
}

} // namespace workbench
