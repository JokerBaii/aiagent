/**
 * @file AuditPromptBuilder.cpp
 * @brief 审计结果的大模型提示构建实现。
 */

#include "cc/llm/AuditPromptBuilder.hpp"
#include "cc/report/JsonReporter.hpp"

namespace cc {
namespace {

[[nodiscard]] std::string systemPrompt() {
    return "你是竞赛项目可信编译器的可选 Brain。只能基于已给出的审计 JSON 做解释、"
           "排序和补证建议；不得编造用户、营收、合作、专利、实验结果或市场数据；"
           "不得推翻 rule_id、可信评分和 blocker/warning 结论；输出中文。";
}

[[nodiscard]] JsonValue compactAuditJson(const JsonValue& root) {
    return JsonValue::Object{{"summary", root.at("summary")},
                             {"cpir", root.at("cpir")},
                             {"findings", root.at("findings")},
                             {"evidence_matches", root.at("evidence_matches")},
                             {"fix_tasks", root.at("fix_tasks")}};
}

} // namespace

std::vector<LlmMessage> AuditPromptBuilder::buildFromResult(const AuditResult& result) const {
    return buildFromAuditJson(JsonReporter{}.toJson(result));
}

std::vector<LlmMessage> AuditPromptBuilder::buildFromAuditJson(const JsonValue& auditJson) const {
    const auto compact = compactAuditJson(auditJson);
    const auto userPrompt =
        std::string{"请作为 Brain 给出：1. 当前最关键的 5 个风险；2. P0 补证优先级；"
                    "3. 答辩前检查清单；4. 哪些内容不能编造。审计 JSON：\n"} +
        writeJson(compact, 2);
    return {LlmMessage{.role = "system", .content = systemPrompt()},
            LlmMessage{.role = "user", .content = userPrompt}};
}

} // namespace cc
