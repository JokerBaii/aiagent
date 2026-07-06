/**
 * @file CompetitionTypeDetector.cpp
 * @brief 竞赛类型识别实现。
 */

#include "cc/cpir/CompetitionTypeDetector.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {

CompetitionTypeResult
CompetitionTypeDetector::detectDetailed(const ProjectInventory& inventory,
                                        const std::vector<TextDocument>& corpus,
                                        CompetitionType requested) const {
    if (requested != CompetitionType::Unknown) {
        return CompetitionTypeResult{
            .type = requested, .confidence = 1.0, .reason = "用户通过 --track 显式指定赛道"};
    }

    std::string allText;
    for (const auto& doc : corpus) {
        allText += util::lowerAscii(doc.text);
        allText += '\n';
    }
    if (util::contains(allText, "三创") || util::contains(allText, "电商")) {
        return CompetitionTypeResult{.type = CompetitionType::Ecommerce,
                                     .confidence = 0.82,
                                     .reason = "材料文本包含三创或电商关键词"};
    }
    if (hasRole(inventory, AssetRole::BusinessPlan) &&
        (hasRole(inventory, AssetRole::FinancialPlan) ||
         hasRole(inventory, AssetRole::MarketResearch))) {
        return CompetitionTypeResult{.type = CompetitionType::BusinessInnovation,
                                     .confidence = 0.80,
                                     .reason = "发现商业计划及财务预测或市场调研等强商业材料"};
    }
    if (hasRole(inventory, AssetRole::SocialPracticeProof) || util::contains(allText, "社会实践")) {
        return CompetitionTypeResult{.type = CompetitionType::SocialPractice,
                                     .confidence = 0.78,
                                     .reason = "发现社会实践证明或社会实践关键词"};
    }
    if (hasRole(inventory, AssetRole::ResearchPaper) ||
        hasRole(inventory, AssetRole::ExperimentData)) {
        return CompetitionTypeResult{.type = CompetitionType::ScientificResearch,
                                     .confidence = 0.76,
                                     .reason = "发现论文或实验数据材料"};
    }
    if (hasRole(inventory, AssetRole::BusinessPlan) ||
        hasRole(inventory, AssetRole::FinancialPlan) ||
        hasRole(inventory, AssetRole::MarketResearch)) {
        return CompetitionTypeResult{.type = CompetitionType::BusinessInnovation,
                                     .confidence = 0.74,
                                     .reason = "发现商业计划、财务预测或市场调研材料"};
    }
    if (hasRole(inventory, AssetRole::SourceCode) || hasRole(inventory, AssetRole::BuildSystem)) {
        return CompetitionTypeResult{.type = CompetitionType::SoftwareProject,
                                     .confidence = 0.72,
                                     .reason = "发现源码或构建入口"};
    }
    return CompetitionTypeResult{.type = CompetitionType::Unknown,
                                 .confidence = 0.0,
                                 .reason = "材料特征不足，无法可靠判断赛道"};
}

CompetitionType CompetitionTypeDetector::detect(const ProjectInventory& inventory,
                                                const std::vector<TextDocument>& corpus,
                                                CompetitionType requested) const {
    return detectDetailed(inventory, corpus, requested).type;
}

} // namespace cc
