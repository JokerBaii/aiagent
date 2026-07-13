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
#include "cc/inventory/MaterialTrustPolicy.hpp"
#include "cc/repair/FixTaskGenerator.hpp"
#include "cc/repair/RepairPlanner.hpp"
#include "cc/rules/RuleEngine.hpp"
#include "cc/rules/RulePackLoader.hpp"
#include "cc/rules/RulePackValidator.hpp"
#include "cc/text/TextExtractionService.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cc {
namespace {

[[nodiscard]] AuditStageOutcome ok(const AuditStageInfo& info, std::string detail) {
    return AuditStageOutcome{
        .name = info.name, .title = info.title, .detail = std::move(detail), .ok = true};
}

struct TextReviewCopy {
    std::string reason;
    std::string suggestion;
};

[[nodiscard]] TextReviewCopy textReviewCopy(const TextDocument& document) {
    const auto path = document.sourceFile.generic_string();
    if (document.status.find("TRUNCATED") != std::string::npos) {
        return {.reason = path + " 比较大，这次只读取了前一部分。未读到的内容还没有核对，所以暂时"
                                 "不把这份文件当作关键证据。",
                .suggestion =
                    "如果它是申报书或证明材料，请提供精简版或可完整读取的版本；普通源码可以"
                    "保留原样。"};
    }
    if (document.status.find("JSON_PARSE_FAILED") != std::string::npos) {
        return {.reason = path + " 不是可正常解析的 JSON，里面的配置暂时无法可靠核对。",
                .suggestion = "检查 JSON 格式；如果它只是示例或构建产物，可以在材料说明中注明。"};
    }
    if (document.status == "EMPTY_OR_UNREADABLE" ||
        document.status.find("YAML_EMPTY") != std::string::npos) {
        return {.reason = path + " 没有读到可用文字，可能是空文件、编码不兼容或二进制内容。",
                .suggestion = "确认文件内容和编码；需要评审的材料请另存为可复制文本的版本。"};
    }
    if (document.status.find("PDF") != std::string::npos ||
        document.status.find("OPENXML") != std::string::npos) {
        return {.reason = path + " 没有提取出足够完整的正文，可能是扫描件或使用了暂不支持的排版。",
                .suggestion = "提供可复制文字的原文件；扫描件请先做 OCR，并人工核对识别结果。"};
    }
    return {.reason = path + " 的正文没有可靠读取完成，因此暂时不能用来支撑关键结论。",
            .suggestion = "请提供可正常打开、可复制文字的版本，并人工确认内容完整。"};
}

void appendTextIntegrityFindings(AuditResult& result) {
    std::size_t index = 0U;
    for (const auto& document : result.corpus) {
        if (!needsTextReview(document)) {
            continue;
        }
        const auto* asset = findAsset(result.inventory, document.sourceFile);
        const bool requiredMaterial =
            asset != nullptr &&
            (asset->role == AssetRole::ProjectDeclaration ||
             asset->role == AssetRole::BusinessPlan || asset->role == AssetRole::ResearchPaper ||
             asset->role == AssetRole::SocialPracticeProof);
        const auto copy = textReviewCopy(document);
        result.findings.push_back(
            {.ruleId = "TEXT_EXTRACTION_REVIEW_" + std::to_string(++index),
             .severity = requiredMaterial ? Severity::Blocker : Severity::Warning,
             .title = "有文件没有读完整",
             .reason = copy.reason,
             .evidence = {document.sourceFile},
             .missingEvidence = {"可可靠读取且经人工确认的材料内容"},
             .fixSuggestion = copy.suggestion});
    }
}

void markUnverifiedWorkspaceFiles(ProjectInventory& inventory,
                                  const std::vector<std::filesystem::path>& files) {
    if (files.empty()) {
        return;
    }
    std::set<std::string> unverified;
    for (const auto& file : files) {
        unverified.insert(file.lexically_normal().generic_string());
    }
    std::size_t marked = 0U;
    for (auto& asset : inventory.assets) {
        if (!unverified.contains(asset.relativePath.lexically_normal().generic_string())) {
            continue;
        }
        asset.workspaceModified = true;
        asset.generated = true;
        asset.role = AssetRole::Generated;
        asset.importance = 1;
        if (std::find(asset.riskFlags.begin(), asset.riskFlags.end(),
                      "WORKSPACE_DRAFT_UNVERIFIED") == asset.riskFlags.end()) {
            asset.riskFlags.emplace_back("WORKSPACE_DRAFT_UNVERIFIED");
        }
        ++marked;
    }
    inventory.roleCounts.clear();
    for (const auto& asset : inventory.assets) {
        ++inventory.roleCounts[asset.role];
    }
    if (marked > 0U) {
        inventory.warnings.push_back(std::to_string(marked) +
                                     " 个 repaired-project 变更被标记为待人工确认草稿，"
                                     "不会作为独立证据或必需材料计分");
    }
}

} // namespace

const std::vector<AuditStageInfo>& StagedAuditEngine::stages() {
    static const std::vector<AuditStageInfo> table = {
        {"inventory_project", "整理项目文件"},       {"extract_text", "读取材料内容"},
        {"detect_competition_type", "判断项目类型"}, {"build_cpir", "整理项目信息"},
        {"extract_claims", "找出需要举证的成果"},    {"match_evidence", "核对证明材料"},
        {"check_consistency", "核对材料表述"},       {"run_rules", "查找提交问题"},
        {"calculate_trust_score", "计算分数"},       {"generate_fix_tasks", "整理修改清单"},
        {"generate_repair_plan", "整理修改方案"}};
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
        markUnverifiedWorkspaceFiles(result_.inventory, options_.unverifiedFiles);
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
        result_.claims = ClaimExtractor{}.extract(result_.corpus, result_.inventory);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "声明 " + std::to_string(result_.claims.size()) + " 条"));
    }
    case 5: {
        result_.evidenceMatches =
            EvidenceMatcher{}.match(result_.claims, result_.inventory, result_.corpus);
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
        appendTextIntegrityFindings(result_);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "发现 " + std::to_string(result_.findings.size()) + " 个需要查看的问题"));
    }
    case 8: {
        result_.trustScore =
            TrustScoreCalculator{}.calculate(result_.inventory, result_.findings,
                                             result_.evidenceMatches, result_.consistencyIssues);
        std::ostringstream detail;
        detail << "当前 " << result_.trustScore.totalScore << " 分，还有 "
               << result_.trustScore.trustDebt << " 分可通过完善材料恢复";
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(ok(info, detail.str()));
    }
    case 9: {
        result_.fixTasks =
            FixTaskGenerator{}.generate(result_.findings, result_.evidenceMatches, result_.claims);
        ++stageIndex_;
        return Result<AuditStageOutcome>::success(
            ok(info, "整理出 " + std::to_string(result_.fixTasks.size()) + " 项修改建议"));
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
        return Result<AuditStageOutcome>::success(ok(info, "修改建议已经整理好，原项目不会被改动"));
    }
    default:
        return Result<AuditStageOutcome>::failure("未知审计步骤");
    }
}

AuditResult StagedAuditEngine::takeResult() {
    return std::move(result_);
}

} // namespace cc
