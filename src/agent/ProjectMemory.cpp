/**
 * @file ProjectMemory.cpp
 * @brief 项目记忆文件初始化实现。
 */

#include "cc/agent/ProjectMemory.hpp"
#include "cc/agent/LifecycleHookManager.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/JsonUtil.hpp"

#include <sstream>

namespace cc {

Result<void> ProjectMemory::init(const std::filesystem::path& projectPath,
                                 CompetitionType track) const {
    std::error_code ec;
    if (!std::filesystem::exists(projectPath, ec) ||
        !std::filesystem::is_directory(projectPath, ec)) {
        return Result<void>::failure("无法初始化项目记忆，项目目录不存在");
    }
    const auto root = projectPath / ".project-trust";
    std::filesystem::create_directories(root, ec);
    if (ec) {
        return Result<void>::failure("无法创建 .project-trust: " + ec.message());
    }

    std::ostringstream md;
    md << "# PROJECT_RULES\n\n";
    md << "- 赛道：" << toString(track) << "\n";
    md << "- 禁止无证据声明直接进入最终报告。\n";
    md << "- 修复必须采用 diff-first 或补证任务模式，不直接覆盖原项目。\n";
    auto written = util::writeTextFile(root / "PROJECT_RULES.md", md.str());
    if (!written.ok()) {
        return written;
    }

    const JsonValue projectRules = JsonValue::Object{
        {"track", toString(track)},
        {"required_materials",
         util::stringArrayToJson({"申报材料", "证据材料", "可复核说明", "补证任务闭环"})},
        {"forbidden_unverified_claims",
         util::stringArrayToJson(
             {"用户数量", "营收", "合作协议", "专利软著", "实验结果", "市场规模"})},
        {"report_requirements",
         util::stringArrayToJson({"规则 ID", "证据来源", "可信评分", "可信债务", "补证任务"})}};
    written = util::writeTextFile(root / "project_rules.json", writeJson(projectRules, 2) + "\n");
    if (!written.ok()) {
        return written;
    }

    const JsonValue permissions = JsonValue::Object{
        {"allowed",
         util::stringArrayToJson({"ReadProjectFiles", "WriteWorkspace", "ExportReport"})},
        {"denied", util::stringArrayToJson(
                       {"ModifyOriginalProject", "ExecuteCommand", "NetworkAccess", "LLMAccess"})}};
    written = util::writeTextFile(root / "permissions.json", writeJson(permissions, 2) + "\n");
    if (!written.ok()) {
        return written;
    }

    const JsonValue hooks = JsonValue::Object{
        {"enabled", util::stringArrayToJson(LifecycleHookManager{}.builtinHooks())}};
    return util::writeTextFile(root / "hooks.json", writeJson(hooks, 2) + "\n");
}

} // namespace cc
