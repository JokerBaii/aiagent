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

[[nodiscard]] QVariantMap messageItem(const QString& role, const QString& text,
                                      const QString& context) {
    QVariantMap item;
    item["role"] = role;
    item["text"] = text;
    item["context"] = context;
    return item;
}

[[nodiscard]] QString toolDisplayName(const std::string& name) {
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
        "inventory_project",       "extract_text",       "detect_competition_type",
        "build_cpir",              "extract_claims",     "match_evidence",
        "check_consistency",       "run_rules",          "calculate_trust_score",
        "generate_fix_tasks",      "generate_repair_plan"};
    return flow;
}

[[nodiscard]] int flowIndex(const std::string& name) {
    const auto& flow = auditToolFlow();
    const auto iter = std::find(flow.begin(), flow.end(), name);
    return iter == flow.end() ? -1 : static_cast<int>(std::distance(flow.begin(), iter));
}

[[nodiscard]] QVariantMap artifactItem(const QString& title, const QString& kind,
                                       const QString& detail) {
    QVariantMap item;
    item["title"] = title;
    item["kind"] = kind;
    item["detail"] = detail;
    return item;
}

[[nodiscard]] QString importStatusText(const std::string& status) {
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

[[nodiscard]] QString permissionDetail(cc::ToolPermission permission, bool allowed) {
    switch (permission) {
    case cc::ToolPermission::ReadProjectFiles:
        return "只读取隔离工作区中的项目副本";
    case cc::ToolPermission::ReadExternalFiles:
        return "默认拒绝读取项目外文件";
    case cc::ToolPermission::WriteWorkspace:
        return "允许写入 .workspaces 会话产物";
    case cc::ToolPermission::ModifyOriginalProject:
        return "默认拒绝覆盖原始项目";
    case cc::ToolPermission::ExecuteCommand:
        return "默认拒绝自由执行脚本或 shell";
    case cc::ToolPermission::NetworkAccess:
        return allowed ? "已由 Brain 确认勾选临时允许" : "默认拒绝联网";
    case cc::ToolPermission::LLMAccess:
        return allowed ? "已由 Brain 确认勾选临时允许" : "默认拒绝调用 LLM";
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

QVariantMap projectContext(const std::optional<cc::AuditResult>& result,
                           const QString& normalizedProjectPath) {
    QVariantMap item;
    if (!result.has_value()) {
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

QVariantList sessionHistory(const std::optional<cc::AuditResult>& result,
                            const std::vector<SessionMessage>& conversation,
                            const QString& normalizedProjectPath) {
    QVariantList items;
    if (!normalizedProjectPath.isEmpty()) {
        items.push_back(messageItem("项目",
                                    QStringLiteral("当前材料包：%1").arg(normalizedProjectPath),
                                    "材料已选择"));
    }
    if (result.has_value()) {
        items.push_back(messageItem(
            "工具",
            QStringLiteral(
                "审计完成：评分 %1/100，必须处理 %2 个，需要关注 %3 个，补证任务 %4 个。")
                .arg(result->trustScore.totalScore)
                .arg(blockerCount(*result))
                .arg(warningCount(*result))
                .arg(result->fixTasks.size()),
            "审计完成"));
        items.push_back(messageItem("顾问", advisorSummary(result), "下一步建议"));
    } else {
        items.push_back(
            messageItem("顾问", "导入项目并开始审计后，我会基于规则结果解释风险和补证优先级。",
                        "等待审计"));
    }
    for (const auto& message : conversation) {
        items.push_back(messageItem(message.role, message.text, message.context));
    }
    return items;
}

QVariantList toolCards(const std::optional<cc::AuditResult>& result,
                       const std::optional<cc::AuditDiff>& auditDiff, bool agentRunning,
                       int activeStep, int completedSteps) {
    QVariantList items;
    const auto names = cc::ToolRegistry{}.builtinToolNames();
    for (const auto& name : names) {
        QVariantMap item;
        item["name"] = toolDisplayName(name);
        item["technicalName"] = stringText(name);
        item["status"] = result.has_value() ? "完成" : "等待";
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

        if (result.has_value()) {
            if (name == "inventory_project") {
                item["detail"] = QStringLiteral("资产 %1 个，输入文件 %2 个")
                                     .arg(result->inventory.assets.size())
                                     .arg(result->context.inputFiles.size());
            } else if (name == "extract_text") {
                item["detail"] = QStringLiteral("可审计文本 %1 份").arg(result->corpus.size());
            } else if (name == "build_cpir") {
                item["detail"] = QStringLiteral("%1，置信度 %2")
                                     .arg(stringText(cc::toString(result->cpir.competitionType)))
                                     .arg(result->cpir.competitionConfidence);
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
            } else if (name == "export_markdown_report" || name == "export_json_report") {
                item["status"] = "可导出";
                item["detail"] = "通过导出页写入用户指定路径";
            } else if (name == "verify_diff") {
                item["status"] = auditDiff.has_value() ? "完成" : "等待";
                item["detail"] =
                    auditDiff.has_value() ? "已生成二次审计差分" : "需要两份 audit.json";
            } else if (name == "explain_audit_finding") {
                item["status"] = "可追问";
                item["detail"] = "在会话里询问某个风险为什么触发";
            } else if (name == "generate_defense_questions") {
                item["status"] = "可追问";
                item["detail"] = "在会话里输入答辩问题或评委会问什么";
            }
        } else if (name == "explain_audit_finding" || name == "generate_defense_questions") {
            item["status"] = "等待";
            item["detail"] = "审计完成后可用";
        }
        items.push_back(item);
    }
    return items;
}

QVariantList permissionCards(bool llmApproved) {
    QVariantList items;
    cc::PermissionGate gate;
    for (const auto permission : gate.permissions()) {
        const auto brainOverride =
            llmApproved && (permission == cc::ToolPermission::NetworkAccess ||
                            permission == cc::ToolPermission::LLMAccess);
        const auto allowed = gate.isAllowed(permission) || brainOverride;
        QVariantMap item;
        item["name"] = permissionName(permission);
        item["technicalName"] = stringText(cc::toString(permission));
        item["status"] = allowed ? "允许" : "默认拒绝";
        item["allowed"] = allowed;
        item["detail"] = permissionDetail(permission, allowed);
        items.push_back(item);
    }
    return items;
}

QVariantList artifacts(const std::optional<cc::AuditResult>& result,
                       const std::optional<cc::AuditDiff>& auditDiff, const QString& llmAdvice) {
    QVariantList items;
    if (!result.has_value()) {
        items.push_back(artifactItem("等待审计产物", "待生成",
                                     "运行审计后生成报告、补证计划和审计数据包"));
        return items;
    }

    items.push_back(artifactItem("审计数据包", "JSON", "包含资产、项目画像、声明证据、风险和评分"));
    items.push_back(
        artifactItem("Markdown 报告", "Markdown", "包含项目概况、风险、补证任务和修复计划"));
    items.push_back(artifactItem("修复计划", "Plan",
                                 QStringLiteral("补证任务 %1 个").arg(result->fixTasks.size())));
    items.push_back(artifactItem("修复建议片段", "Diff", "只生成建议，不覆盖原项目"));
    if (auditDiff.has_value()) {
        items.push_back(artifactItem("二次审计差分", "JSON", stringText(auditDiff->summary)));
    }
    if (!llmAdvice.isEmpty()) {
        items.push_back(
            artifactItem("Brain 建议", "LLM", "显式授权后生成，只作为解释和补证优先级建议"));
    }
    return items;
}

QString advisorSummary(const std::optional<cc::AuditResult>& result) {
    if (!result.has_value()) {
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
