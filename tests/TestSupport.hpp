/**
 * @file TestSupport.hpp
 * @brief 极简测试辅助。
 */

#pragma once

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

inline void requireTrue(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline std::filesystem::path sourceDir() {
    return std::filesystem::path{CONTEST_SOURCE_DIR};
}

void runCoreTests();
void runLoaderTests();
void runInventoryTests();
void runTextTests();
void runCpirTests();
void runClaimTests();
void runEvidenceTests();
void runConsistencyTests();
void runRulesTests();
void runRuleConditionEvaluatorTests();
void runAuditTests();
void runRepairTests();
void runReportTests();
void runAgentTests();
void runLlmTests();
void runLlmProviderProfileTests();
void runWorkspaceEditorTests();
