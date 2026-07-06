/**
 * @file RepairDiff.cpp
 * @brief diff-first 修复产物生成实现。
 */

#include "cc/repair/RepairDiff.hpp"
#include "cc/util/StringUtil.hpp"

#include <sstream>

namespace cc {

std::string RepairDiff::generate(const std::vector<FixTask>& tasks, const CPIR& cpir) const {
    std::ostringstream content;
    content << "# " << (cpir.projectName.empty() ? "项目" : cpir.projectName)
            << " 补证任务清单\n\n";
    content << "本文件应生成到 repaired/PROJECT_TRUST_FIX_TASKS.md，不能覆盖原项目文件。\n\n";
    for (const auto& task : tasks) {
        content << "## " << task.taskId << " " << task.title << "\n\n";
        content << "- 优先级：" << task.priority << "\n";
        content << "- 原因：" << task.reason << "\n";
        content << "- 关联规则：" << util::join(task.affectedRules, "、") << "\n";
        content << "- 需要补充：" << util::join(task.requiredMaterial, "、") << "\n\n";
    }

    const auto text = content.str();
    std::ostringstream diff;
    diff << "diff --git a/repaired/PROJECT_TRUST_FIX_TASKS.md "
            "b/repaired/PROJECT_TRUST_FIX_TASKS.md\n";
    diff << "new file mode 100644\n";
    diff << "index 0000000..1111111\n";
    diff << "--- /dev/null\n";
    diff << "+++ b/repaired/PROJECT_TRUST_FIX_TASKS.md\n";
    for (const auto& line : util::splitLines(text)) {
        diff << "+" << line << "\n";
    }
    return diff.str();
}

} // namespace cc
