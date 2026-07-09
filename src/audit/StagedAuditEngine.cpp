/**
 * @file StagedAuditEngine.cpp
 * @brief 可分步执行的核心审计引擎实现。
 */

#include "cc/audit/StagedAuditEngine.hpp"

#include "cc/audit/TrustScoreCalculator.hpp"
#include "cc/claim/ClaimExtractor.hpp"
#include "cc/consistency/ConsistencyChecker.hpp"
#include "cc/cpir/CPIRBuilder.hpp"
#include "cc/cpir/CompetitionTypeDetector.hpp"
#include "cc/evidence/EvidenceMatcher.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/repair/FixTaskGenerator.hpp"
#include "cc/repair/RepairPlanner.hpp"
#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RulePackLoader.hpp"
#include "cc/rules/RulePackValidator.hpp"
#include "cc/text/TextExtractionService.hpp"

#include <sstream>

namespace cc {
namespace {

[[nodiscard]] AuditStageOutcome ok(const AuditStageInfo& info, std::string detail) {
    return AuditStageOutcome{
        .name = info.name, .title = info.title, .detail = std::move(detail), .ok = true};
}

} // namespace

const std::vector<AuditStageInfo>& StagedAuditEngine::stages() {
    static const std::vector<AuditStageInfo> table = {
        {"inventory_project", "整理材料"},       {"extract_text", "读取文本"},
        {"detect_competition_type", "判断赛道"}, {"build_cpir", "生成项目画像"},
        {"extract_claims", "提取关键声明"},      {"match_evidence", "匹配证据"},
        {"check_consistency", "检查一致性"},     {"run_rules", "执行规则"},
        {"calculate_trust_score", "计算评分"},   {"generate_fix_tasks", "生成补证任务"},
        {"generate_repair_plan", "整理修复计划"}};
    return table;
}

void StagedAuditEngine::reset(const ProjectContext& context, const AuditOptions& options) {
    context_ = context;
    options_ = options;
    result_ = AuditResult{};
    result_.context = context;
    stageIndex_ = 0;
    started_ = true;
}

bool StagedAuditEngine::hasNext() const {
    return started_ && stageIndex_ < stages().size();
}

std::size_t StagedAuditEngine::completedStages() const {
    return stageIndex_;
}

Result<AuditStageOutcome> StagedAuditEngine::advance() {
    if (!started_) {
        return Result<AuditStageOutcome>::failure("审计引擎尚未绑定项目上下文");
    }
    if (stageIndex_ >= stages().size()) {
        return Result<AuditStageOutcome>::failure("审计步骤已全部完成");
    }

    const auto& info = stages().at(stageIndex_);
    switch (stageIndex_) {
    case 0: {
        auto inventory = InventoryEngine{}.build(context_);
        if (!inventory.ok()) {
            return Result<AuditStageOutcome>::failure(inventory.error());
        }
        result_.inventory = std::move(inventory.value());
        std::ostringstream detail;
        detail << "资产 " << result_.inventory.assets.size() << " 个，输入文件 "
               << context_.inputFiles.size() << " 个";
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, detail.str()));
    }
    case 1: {
        auto corpus = TextExtractionService{}.extract(result_.inventory);
        if (!corpus.ok()) {
            return Result<AuditStageOutcome>::failure(corpus.error());
        }
        result_.corpus = std::move(corpus.value());
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "可审计文本 " + std::to_string(result_.corpus.size()) + " 份"));
    }
    case 2: {
        typeResult_ = CompetitionTypeDetector{}.detectDetailed(result_.inventory, result_.corpus,
                                                               options_.track);
        std::ostringstream detail;
        detail << toString(typeResult_.type) << "，置信度 " << typeResult_.confidence;
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, detail.str()));
    }
    case 3: {
        result_.cpir = CPIRBuilder{}.build(result_.inventory, result_.corpus, typeResult_);
        std::ostringstream detail;
        detail << "已生成 CPIR，缺失字段 " << result_.cpir.missingFields.size() << " 项";
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, detail.str()));
    }
    case 4: {
        result_.claims = ClaimExtractor{}.extract(result_.corpus);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "声明 " + std::to_string(result_.claims.size()) + " 条"));
    }
    case 5: {
        result_.evidenceMatches = EvidenceMatcher{}.match(result_.claims, result_.inventory);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "证据匹配 " + std::to_string(result_.evidenceMatches.size()) + " 条"));
    }
    case 6: {
        result_.consistencyIssues =
            ConsistencyChecker{}.check(result_.cpir, result_.inventory, result_.claims);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "一致性问题 " + std::to_string(result_.consistencyIssues.size()) + " 个"));
    }
    case 7: {
        auto rules =
            RulePackLoader{}.loadDirectory(options_.rulesDir, result_.cpir.competitionType);
        if (!rules.ok()) {
            return Result<AuditStageOutcome>::failure(rules.error());
        }
        auto valid = RulePackValidator{}.validate(rules.value());
        if (!valid.ok()) {
            return Result<AuditStageOutcome>::failure(valid.error());
        }
        result_.findings =
            RuleEngine{}.evaluate(rules.value(), result_.inventory, result_.cpir, result_.claims,
                                  result_.evidenceMatches, result_.consistencyIssues);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "规则风险 " + std::to_string(result_.findings.size()) + " 个"));
    }
    case 8: {
        result_.trustScore =
            TrustScoreCalculator{}.calculate(result_.inventory, result_.findings,
                                             result_.evidenceMatches, result_.consistencyIssues);
        std::ostringstream detail;
        detail << "可信评分 " << result_.trustScore.totalScore << "，可信债务 "
               << result_.trustScore.trustDebt;
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, detail.str()));
    }
    case 9: {
        result_.fixTasks = FixTaskGenerator{}.generate(result_.findings, result_.evidenceMatches);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "补证任务 " + std::to_string(result_.fixTasks.size()) + " 个"));
    }
    case 10: {
        result_.repairPlan = RepairPlanner{}.plan(result_.fixTasks, result_.cpir);
        std::vector<std::string> outputs;
        outputs.reserve(stages().size());
        for (const auto& stage : stages()) {
            outputs.push_back(stage.name);
        }
        result_.toolOutputs = std::move(outputs);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, "已生成修复建议，不覆盖原项目"));
    }
    default:
        return Result<AuditStageOutcome>::failure("未知审计步骤");
    }
}

AuditResult StagedAuditEngine::takeResult() {
    return std::move(result_);
}

} // namespace cc
