/**
 * @file AgentCommandRouter.cpp
 * @brief 会话输入路由实现。
 */

#include "cc/agent/AgentCommandRouter.hpp"

#include "cc/util/StringUtil.hpp"

namespace cc {
namespace {

[[nodiscard]] bool startsWithSlash(const std::string& text) {
    return !text.empty() && text.front() == '/';
}

[[nodiscard]] std::string commandName(const std::string& text) {
    const auto space = text.find(' ');
    const auto command = space == std::string::npos ? text.substr(1U) : text.substr(1U, space - 1U);
    return util::lowerAscii(command);
}

[[nodiscard]] std::string commandArgs(const std::string& text) {
    const auto space = text.find(' ');
    return space == std::string::npos ? std::string{} : util::trim(text.substr(space + 1U));
}

[[nodiscard]] AgentCommand task(std::string prompt, std::string context) {
    return AgentCommand{.kind = AgentCommandKind::RunAgentTask,
                        .prompt = std::move(prompt),
                        .context = std::move(context)};
}

} // namespace

Result<AgentCommand> AgentCommandRouter::route(const std::string& input) const {
    const auto text = util::trim(input);
    if (text.empty()) {
        return Result<AgentCommand>::success(AgentCommand{});
    }
    if (!startsWithSlash(text)) {
        return Result<AgentCommand>::success(task(text, "来自输入框"));
    }

    const auto command = commandName(text);
    if (command == "audit" || command == "run-audit") {
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::RunAudit, .prompt = text, .context = "命令"});
    }
    if (command == "plan") {
        const auto args = commandArgs(text);
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::PlanTask, .prompt = args, .context = "/plan"});
    }
    if (command == "ask" || command == "code" || command == "bypass") {
        const auto args = commandArgs(text);
        if (args.empty()) {
            return Result<AgentCommand>::success(
                AgentCommand{.kind = AgentCommandKind::SetPermissionMode,
                             .prompt = command,
                             .context = "/" + command});
        }
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::RunModePrefixedTask,
                         .prompt = args,
                         .context = "/" + command});
    }
    if (command == "permissions" || command == "permission") {
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::SetPermissionMode,
                         .prompt = commandArgs(text),
                         .context = "/permissions"});
    }
    if (command == "status") {
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::ShowStatus, .prompt = text, .context = "命令"});
    }
    if (command == "compact") {
        return Result<AgentCommand>::success(AgentCommand{
            .kind = AgentCommandKind::CompactContext, .prompt = text, .context = "命令"});
    }
    if (command == "new" || command == "clear") {
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::NewSession, .prompt = text, .context = "命令"});
    }
    if (command == "agent" || command == "task") {
        const auto args = commandArgs(text);
        if (args.empty()) {
            return Result<AgentCommand>::failure("/" + command + " 需要一个任务描述");
        }
        return Result<AgentCommand>::success(task(args, "/" + command));
    }
    if (command == "optimize" || command == "repair") {
        auto goal = commandArgs(text);
        if (goal.empty()) {
            goal = "根据确定性审计的 P0/P1 缺点修改项目。先读取相关文件，只在 repaired "
                   "project 中做有依据的精确编辑或新建说明；读回变更、列出 diff，并对 repaired "
                   "project 执行二次审计。不得伪造用户、营收、合作、专利、实验或市场数据。";
        }
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::RunModePrefixedTask,
                         .prompt = std::move(goal),
                         .context = "/optimize"});
    }
    if (command == "help") {
        return Result<AgentCommand>::success(
            AgentCommand{.kind = AgentCommandKind::ShowHelp, .prompt = text, .context = "命令"});
    }
    return Result<AgentCommand>::failure("未知命令: /" + command);
}

} // namespace cc
