/**
 * @file AgentCommandRouter.hpp
 * @brief 会话输入到智能体命令的路由。
 */

#pragma once

#include "cc/core/Result.hpp"

#include <string>

namespace cc {

/**
 * @brief Workbench composer 支持的顶层命令类型。
 *
 * 命令路由只识别显式 slash command；自然语言一律作为 agent task 交给 Brain 或本地
 * runtime，不再靠关键词假装理解用户意图。
 */
enum class AgentCommandKind {
    Empty,
    RunAudit,
    RunAgentTask,
    PlanTask,
    SetPermissionMode,
    ShowStatus,
    CompactContext,
    NewSession,
    ShowHelp,
    RunModePrefixedTask
};

/**
 * @brief 一次会话输入解析后的命令。
 */
struct AgentCommand {
    AgentCommandKind kind{AgentCommandKind::Empty};
    std::string prompt;
    std::string context;
};

/**
 * @brief 将 composer 输入解析为明确命令或普通 agent task。
 */
class AgentCommandRouter {
  public:
    /**
     * @brief 解析用户输入。
     *
     * 主入口支持 `/audit`、`/agent <task>`、`/optimize`、`/status`、`/compact`、`/clear` 和
     * `/help`；`/ask`、`/plan`、`/code`、`/bypass`、`/full`、`/permissions <mode>` 保留为
     * 设置页/高级入口兼容能力。其他非空文本作为普通智能体任务处理。
     */
    [[nodiscard]] Result<AgentCommand> route(const std::string& input) const;
};

} // namespace cc
