/**
 * @file ToolRegistry.cpp
 * @brief 内置工具注册信息实现。
 */

#include "cc/agent/ToolRegistry.hpp"

namespace cc {

std::vector<std::string> ToolRegistry::builtinToolNames() const {
    return {"inventory_project",
            "extract_text",
            "detect_competition_type",
            "build_cpir",
            "extract_claims",
            "match_evidence",
            "check_consistency",
            "run_rules",
            "calculate_trust_score",
            "generate_fix_tasks",
            "generate_repair_plan",
            "export_markdown_report",
            "export_json_report",
            "verify_diff",
            "explain_audit_finding",
            "generate_defense_questions"};
}

} // namespace cc
