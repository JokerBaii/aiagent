/**
 * @file AuditModels.hpp
 * @brief 声明、证据、规则、评分和审计结果模型。
 */

#pragma once

#include "cc/core/JsonValue.hpp"
#include "cc/core/ProjectModels.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 从材料中抽取出的承诺性声明。
 *
 * claimId 和 sourceFile 用于把报告结论追溯回原始材料，避免无来源的风险判断进入最终报告。
 */
struct ProjectClaim {
    std::string claimId;
    std::string claimText;
    ClaimType claimType{ClaimType::Unknown};
    std::filesystem::path sourceFile;
    double confidence{0.0};
    std::string initialRisk;
};

/**
 * @brief 声明与证据的匹配结果。
 *
 * Unsupported/NeedReview 会直接影响可信评分和补证任务，因此必须保留缺失证据和原因。
 */
struct EvidenceMatch {
    std::string claimId{};
    EvidenceStatus status{EvidenceStatus::Unsupported};
    std::vector<std::filesystem::path> evidenceFiles{};
    std::vector<std::string> missingEvidence{};
    std::string reason{};
};

/**
 * @brief 跨材料一致性问题。
 *
 * 一致性问题不直接伪造修改内容，只指出受影响文件和修复建议，供 RepairPlanner 生成补证计划。
 */
struct ConsistencyIssue {
    std::string issueId;
    Severity severity{Severity::Warning};
    std::string description;
    std::vector<std::filesystem::path> affectedFiles;
    std::string fixSuggestion;
};

/**
 * @brief JSON 规则包中的单条审计规则。
 *
 * 规则必须带 ruleId、中文描述、触发条件和修复任务，RuleEngine 执行后才能保持可解释。
 */
struct AuditRule {
    std::string ruleId;
    std::string name;
    std::string track;
    Severity severity{Severity::Warning};
    std::string target;
    std::string description;
    JsonValue condition;
    std::string failReason;
    std::string fixTask;
};

/**
 * @brief 规则触发后的审计风险项。
 *
 * 每个 finding 必须绑定 ruleId 或证据缺口，报告不得输出不可追溯的主观结论。
 */
struct AuditFinding {
    std::string ruleId;
    Severity severity{Severity::Warning};
    std::string title;
    std::string reason;
    std::vector<std::filesystem::path> evidence;
    std::vector<std::string> missingEvidence;
    std::string fixSuggestion;
};

/**
 * @brief 可信评分扣分项。
 *
 * 扣分必须关联规则和维度，禁止魔法分数直接修改总分。
 */
struct ScorePenalty {
    std::string ruleId;
    int points{0};
    std::string dimension;
    std::string reason;
};

/**
 * @brief 可信评分和可信债务。
 *
 * totalScore 是确定性规则计算结果，LLM Brain 只能解释不能覆盖该结果。
 */
struct TrustScore {
    int totalScore{100};
    int trustDebt{0};
    std::map<std::string, int> dimensions;
    std::vector<ScorePenalty> penalties;
};

/**
 * @brief 可执行补证任务。
 *
 * 任务必须说明原因、材料和影响规则，避免输出空泛的“继续完善”建议。
 */
struct FixTask {
    std::string taskId{};
    std::string title{};
    std::string priority{};
    std::string reason{};
    std::vector<std::string> requiredMaterial{};
    std::vector<std::string> affectedRules{};
    std::vector<std::filesystem::path> relatedFiles{};
};

/**
 * @brief 修复计划和 diff-first 产物。
 *
 * 修复计划只写入工作区或报告，不直接覆盖 originalRoot。
 */
struct RepairPlan {
    std::vector<FixTask> tasks;
    std::string markdown;
    std::string diffText;
};

/**
 * @brief 端到端审计结果。
 *
 * 该结构保存每个核心步骤的输出，是报告导出、二次审计和 SessionStore 的共同数据源。
 */
struct AuditResult {
    ProjectContext context;
    ProjectInventory inventory;
    std::vector<TextDocument> corpus;
    CPIR cpir;
    std::vector<ProjectClaim> claims;
    std::vector<EvidenceMatch> evidenceMatches;
    std::vector<ConsistencyIssue> consistencyIssues;
    std::vector<AuditFinding> findings;
    TrustScore trustScore;
    std::vector<FixTask> fixTasks;
    RepairPlan repairPlan;
    std::vector<std::string> toolOutputs;
};

/**
 * @brief 可复核审计会话。
 *
 * toolOutputs 用于证明审计工具链路已进入会话记录，便于后续复查。
 */
struct AuditSession {
    std::string sessionId;
    ProjectContext context;
    AuditResult result;
    std::vector<std::string> toolOutputs;
};

/**
 * @brief 两次审计结果的差分摘要。
 *
 * 差分只比较已生成的 audit.json，不推断或伪造修复效果。
 */
struct AuditDiff {
    int oldScore{0};
    int newScore{0};
    int oldTrustDebt{0};
    int newTrustDebt{0};
    int oldBlockers{0};
    int newBlockers{0};
    int oldWarnings{0};
    int newWarnings{0};
    double oldEvidenceCoverage{0.0};
    double newEvidenceCoverage{0.0};
    int oldMaterialCompleteness{0};
    int newMaterialCompleteness{0};
    int oldConsistencyScore{0};
    int newConsistencyScore{0};
    int oldFixTaskCount{0};
    int newFixTaskCount{0};
    std::string summary;
};

/**
 * @brief 审计运行选项。
 *
 * track 支持用户显式指定赛道；rulesDir 指向 JSON 规则包目录。
 */
struct AuditOptions {
    CompetitionType track{CompetitionType::Unknown};
    std::filesystem::path rulesDir{"rules"};
    std::vector<std::filesystem::path> unverifiedFiles;
};

} // namespace cc
