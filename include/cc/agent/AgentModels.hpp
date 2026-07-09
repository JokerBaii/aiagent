/**
 * @file AgentModels.hpp
 * @brief 受控智能体运行时的数据协议。
 */

#pragma once

#include "cc/core/AuditModels.hpp"
#include "cc/core/JsonValue.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 可被 Brain 选择的工具说明。
 *
 * 工具说明同时暴露权限和输入/输出 schema，避免模型把平台理解成可以任意执行
 * shell 的黑箱电脑。
 */
struct AgentToolSpec {
    std::string name;
    std::string description;
    ToolPermission permission{ToolPermission::ReadProjectFiles};
    JsonValue inputSchema;
    JsonValue outputSchema;
};

/**
 * @brief Brain 规划出的单次工具调用。
 */
struct AgentToolCall {
    std::string id;
    std::string name;
    std::string reason;
    JsonValue input;
};

/**
 * @brief 工具执行后的观察结果。
 *
 * observation 会进入会话流和 trace，使用户能复核模型到底读了哪些文件、生成了
 * 哪些工作区产物。
 */
struct AgentObservation {
    std::string callId;
    std::string toolName;
    bool ok{false};
    std::string summary;
    JsonValue output;
};

/**
 * @brief 一轮智能体计划。
 */
struct AgentPlan {
    std::string summary;
    std::vector<AgentToolCall> calls;
};

/**
 * @brief 跨回合提供给 Brain 的精简对话消息。
 */
struct AgentConversationMessage {
    std::string role;
    std::string content;
};

/**
 * @brief Brain 在一轮工具循环中的下一步决策类型。
 */
enum class AgentDecisionKind { ToolCall, FinalAnswer };

/**
 * @brief Brain 根据当前观察结果给出的下一步决策。
 *
 * ToolCall 表示继续调用一个注册工具；FinalAnswer 表示停止工具循环并向用户收束。
 */
struct AgentDecision {
    AgentDecisionKind kind{AgentDecisionKind::ToolCall};
    std::string summary;
    AgentToolCall call;
    std::string finalAnswer;
};

/**
 * @brief 智能体回合中的可展示事件类型。
 */
enum class AgentEventKind { Plan, Tool, Assistant, System };

/**
 * @brief 智能体回合事件。
 *
 * Workbench 只负责展示这些事件，不再在 Controller 中临时拼装工具轨迹。
 */
struct AgentEvent {
    AgentEventKind kind{AgentEventKind::System};
    std::string role;
    std::string text;
    std::string context;
    JsonValue payload;
};

/**
 * @brief 智能体运行请求。
 *
 * projectRoot 指向允许读取的项目副本或用户选择的项目目录；workspaceRoot 指向智能体
 * 可以写入的会话工作区。auditResult 是只读上下文，不允许模型覆盖最终评分。
 */
struct AgentRunRequest {
    std::string userGoal;
    std::filesystem::path projectRoot;
    std::filesystem::path workspaceRoot;
    AuditOptions auditOptions;
    std::vector<AgentConversationMessage> conversationHistory;
    std::string permissionMode{"ask"};
    bool requireAudit{false};
    bool allowReadExternal{false};
    bool allowModifyOriginal{false};
    bool allowExecuteCommand{false};
    bool allowNetwork{false};
    bool allowLlm{false};
    const AuditResult* auditResult{nullptr};
};

/**
 * @brief 一轮智能体运行结果。
 */
struct AgentRunResult {
    AgentPlan plan;
    std::vector<AgentObservation> observations;
    std::vector<AgentEvent> events;
    std::string finalAnswer;
    JsonValue trace;
    std::optional<AuditResult> auditResult;
};

/**
 * @brief 单次工具执行的展示观察和可选强类型审计结果。
 *
 * run_project_audit 会返回全部阶段观察及 AuditResult；普通工具只返回一个观察。
 */
struct AgentToolExecution {
    std::vector<AgentObservation> observations;
    std::optional<AuditResult> auditResult;
};

/** @brief 将回合事件类型转为稳定字符串。 */
[[nodiscard]] std::string toString(AgentEventKind kind);
/** @brief 将 Brain 决策类型转为稳定字符串。 */
[[nodiscard]] std::string toString(AgentDecisionKind kind);

} // namespace cc
