#include "cc/agent/ProjectMemory.hpp"

#include "cc/agent/LifecycleHookManager.hpp"
#include "cc/core/JsonValue.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/JsonUtil.hpp"

#include <filesystem>
#include <sstream>

namespace cc {
namespace {

constexpr std::size_t kMaximumInstructionsBytes = 64U * 1024U;

[[nodiscard]] std::filesystem::path memoryRoot(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / ".project-trust";
}

} // namespace

Result<void> ProjectMemory::init(const std::filesystem::path& workspaceRoot,
                                 CompetitionType track) const {
    std::error_code ec;
    if (!std::filesystem::is_directory(workspaceRoot, ec)) {
        return Result<void>::failure("无法初始化项目记忆，工作区不存在");
    }
    const auto root = memoryRoot(workspaceRoot);
    std::filesystem::create_directories(root, ec);
    if (ec) {
        return Result<void>::failure("无法创建项目记忆目录: " + ec.message());
    }

    std::ostringstream instructions;
    instructions << "# 项目审计约束\n\n";
    instructions << "- 赛道：" << toString(track) << "\n";
    instructions
        << "- 声明必须绑定可复核证据；草稿、智能体新建文件和待复核文本不能充当事实证据。\n";
    instructions << "- 智能体默认直接读取用户选择的原项目；完全访问模式可直接写入原项目并执行"
                    " Shell/Bash，相关风险由用户确认承担。\n";
    instructions << "- 最终评分由确定性规则引擎生成，大模型只能解释和提出建议。\n";
    auto written = util::writeTextFile(root / "PROJECT_RULES.md", instructions.str());
    if (!written.ok()) {
        return written;
    }

    const JsonValue projectRules = JsonValue::Object{
        {"track", toString(track)},
        {"evidence_policy", "Only verified original evidence may support claims"},
        {"repair_policy", "Full mode may edit the user-selected original project"},
        {"score_policy", "Deterministic rules remain authoritative"}};
    written = util::writeTextFile(root / "project_rules.json", writeJson(projectRules, 2) + "\n");
    if (!written.ok()) {
        return written;
    }

    const JsonValue permissions = JsonValue::Object{
        {"always_allowed",
         util::stringArrayToJson({"ReadProjectFiles", "ReadExternalFiles", "WriteWorkspace",
                                  "ModifyOriginalProject", "ExecuteCommand", "NetworkAccess",
                                  "LLMAccess", "ExportReport"})},
        {"explicit_consent_required", JsonValue::Array{}},
        {"always_denied", JsonValue::Array{}}};
    written = util::writeTextFile(root / "permissions.json", writeJson(permissions, 2) + "\n");
    if (!written.ok()) {
        return written;
    }

    const JsonValue hooks = JsonValue::Object{
        {"enabled", util::stringArrayToJson(LifecycleHookManager{}.builtinHooks())}};
    return util::writeTextFile(root / "hooks.json", writeJson(hooks, 2) + "\n");
}

Result<std::string>
ProjectMemory::loadInstructions(const std::filesystem::path& workspaceRoot) const {
    const auto path = memoryRoot(workspaceRoot) / "PROJECT_RULES.md";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return Result<std::string>::failure("项目记忆尚未初始化");
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaximumInstructionsBytes) {
        return Result<std::string>::failure("项目记忆文件超过读取上限或无法读取");
    }
    auto content = util::readFileLimited(path, kMaximumInstructionsBytes + 1U);
    if (content.empty() || content.size() > kMaximumInstructionsBytes) {
        return Result<std::string>::failure("项目记忆为空或超过读取上限");
    }
    return Result<std::string>::success(std::move(content));
}

} // namespace cc
