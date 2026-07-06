/**
 * @file RepairPlanner.cpp
 * @brief 修复计划生成实现。
 */

#include "cc/repair/RepairPlanner.hpp"
#include "cc/repair/RepairDiff.hpp"
#include "cc/util/StringUtil.hpp"

#include <sstream>

namespace cc {

RepairPlan RepairPlanner::plan(const std::vector<FixTask>& tasks, const CPIR& cpir) const {
    RepairPlan plan;
    plan.tasks = tasks;
    std::ostringstream output;
    output << "# 修复计划\n\n";
    output << "项目：" << cpir.projectName << "\n\n";
    output << "> 本计划只生成补证任务，不生成虚假用户、营收、合作、专利、实验结果或市场数据。\n\n";
    for (const auto& task : tasks) {
        output << "## " << task.taskId << " " << task.title << "\n\n";
        output << "- 优先级：" << task.priority << "\n";
        output << "- 原因：" << task.reason << "\n";
        output << "- 关联规则：" << util::join(task.affectedRules, "、") << "\n";
        output << "- 需要补充：" << util::join(task.requiredMaterial, "、") << "\n\n";
    }
    if (tasks.empty()) {
        output << "当前未生成补证任务。仍建议人工复核关键证明材料。\n";
    }
    plan.markdown = output.str();
    plan.diffText = RepairDiff{}.generate(tasks, cpir);
    return plan;
}

} // namespace cc
