#include "cc/cpir/CompetitionTypeDetector.hpp"

#include "cc/core/Enums.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/inventory/MaterialTrustPolicy.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <sstream>

namespace cc {
namespace {

using Scores = std::map<CompetitionType, int>;

void add(Scores& scores, CompetitionType type, int points) {
    scores[type] += points;
}

void addIfContains(Scores& scores, CompetitionType type, const std::string& text,
                   std::initializer_list<std::string_view> keywords, int points) {
    for (const auto keyword : keywords) {
        if (util::contains(text, keyword)) {
            add(scores, type, points);
            return;
        }
    }
}

[[nodiscard]] bool trustedDocument(const TextDocument& document,
                                   const ProjectInventory& inventory) {
    if (!isReliableTextDocument(document) || isExcludedTruthPath(document.sourceFile)) {
        return false;
    }
    if (inventory.assets.empty()) {
        return true;
    }
    const auto* asset = findAsset(inventory, document.sourceFile);
    return asset != nullptr && isTrustedNarrativeAsset(*asset);
}

[[nodiscard]] std::string boundedTrustedText(const ProjectInventory& inventory,
                                             const std::vector<TextDocument>& corpus) {
    constexpr std::size_t kPerDocumentBytes = 64U * 1024U;
    constexpr std::size_t kTotalBytes = 512U * 1024U;
    std::string text;
    text.reserve(std::min(kTotalBytes, corpus.size() * 1024U));
    for (const auto& document : corpus) {
        if (!trustedDocument(document, inventory) || text.size() >= kTotalBytes) {
            continue;
        }
        const auto remaining = kTotalBytes - text.size();
        const auto count = std::min({document.text.size(), kPerDocumentBytes, remaining});
        text.append(document.text, 0U, count);
        text.push_back('\n');
    }
    return util::lowerAscii(std::move(text));
}

void scoreAssets(const ProjectInventory& inventory, Scores& scores) {
    if (hasRole(inventory, AssetRole::BusinessPlan)) {
        add(scores, CompetitionType::BusinessInnovation, 2);
    }
    if (hasRole(inventory, AssetRole::FinancialPlan)) {
        add(scores, CompetitionType::BusinessInnovation, 2);
    }
    if (hasRole(inventory, AssetRole::MarketResearch)) {
        add(scores, CompetitionType::BusinessInnovation, 2);
    }
    if (hasRole(inventory, AssetRole::SourceCode)) {
        add(scores, CompetitionType::SoftwareProject, 2);
    }
    if (hasRole(inventory, AssetRole::BuildSystem)) {
        add(scores, CompetitionType::SoftwareProject, 2);
    }
    if (hasRole(inventory, AssetRole::DeploymentDoc)) {
        add(scores, CompetitionType::SoftwareProject, 1);
    }
    if (hasRole(inventory, AssetRole::ResearchPaper)) {
        add(scores, CompetitionType::ScientificResearch, 3);
    }
    if (hasRole(inventory, AssetRole::ExperimentData)) {
        add(scores, CompetitionType::ScientificResearch, 2);
    }
    if (hasRole(inventory, AssetRole::SocialPracticeProof)) {
        add(scores, CompetitionType::SocialPractice, 3);
    }
    if (hasRole(inventory, AssetRole::ModelArtifact)) {
        add(scores, CompetitionType::AiApplication, 4);
    }
}

void scoreText(const std::string& text, Scores& scores) {
    addIfContains(scores, CompetitionType::Ecommerce, text,
                  {"三创", "电子商务", "电商平台", "直播带货", "跨境电商"}, 5);
    addIfContains(scores, CompetitionType::AiApplication, text,
                  {"人工智能", "机器学习", "深度学习", "大模型", "神经网络", "智能体"}, 4);
    addIfContains(scores, CompetitionType::EngineeringProduct, text,
                  {"硬件样机", "工程设计", "机械结构", "电路板", "传感器", "制造工艺"}, 4);
    addIfContains(scores, CompetitionType::ScientificResearch, text,
                  {"科学研究", "实验设计", "对照实验", "baseline", "论文", "研究方法"}, 4);
    addIfContains(scores, CompetitionType::SocialPractice, text,
                  {"社会实践", "实地调研", "三下乡", "社区服务", "实践团队"}, 4);
    addIfContains(scores, CompetitionType::PublicWelfare, text,
                  {"公益项目", "志愿服务", "无偿服务", "弱势群体", "公益慈善"}, 5);
    addIfContains(scores, CompetitionType::ComprehensiveInnovation, text,
                  {"挑战杯", "综合创新", "创新创业大赛", "科技发明制作"}, 4);
    addIfContains(scores, CompetitionType::BusinessInnovation, text,
                  {"商业计划", "商业模式", "营收预测", "市场规模", "客户获取"}, 3);
    addIfContains(scores, CompetitionType::SoftwareProject, text,
                  {"软件系统", "应用程序", "技术架构", "部署说明", "系统平台"}, 3);
}

} // namespace

CompetitionTypeResult
CompetitionTypeDetector::detectDetailed(const ProjectInventory& inventory,
                                        const std::vector<TextDocument>& corpus,
                                        CompetitionType requested) const {
    if (requested != CompetitionType::Unknown) {
        return CompetitionTypeResult{
            .type = requested, .confidence = 1.0, .reason = "用户显式指定赛道"};
    }

    Scores scores;
    constexpr std::array<CompetitionType, 9> types = {
        CompetitionType::BusinessInnovation,
        CompetitionType::SoftwareProject,
        CompetitionType::EngineeringProduct,
        CompetitionType::ScientificResearch,
        CompetitionType::SocialPractice,
        CompetitionType::PublicWelfare,
        CompetitionType::Ecommerce,
        CompetitionType::AiApplication,
        CompetitionType::ComprehensiveInnovation,
    };
    for (const auto type : types) {
        scores[type] = 0;
    }
    scoreAssets(inventory, scores);
    const auto text = boundedTrustedText(inventory, corpus);
    scoreText(text, scores);

    std::vector<std::pair<CompetitionType, int>> ranked(scores.begin(), scores.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return static_cast<int>(left.first) < static_cast<int>(right.first);
    });
    const auto best = ranked.front();
    const auto second = ranked.size() > 1U ? ranked[1U].second : 0;
    if (best.second < 3 || (best.second == second && best.second < 6)) {
        return CompetitionTypeResult{.type = CompetitionType::Unknown,
                                     .confidence = 0.0,
                                     .reason = "可信材料的赛道特征不足或存在并列，需要人工确认"};
    }

    const auto margin = std::max(best.second - second, 0);
    const auto confidence =
        std::clamp(0.55 + static_cast<double>(std::min(best.second, 10)) * 0.03 +
                       static_cast<double>(margin) * 0.02,
                   0.55, 0.90);
    std::ostringstream reason;
    reason << "可信材料对“" << toString(best.first) << "”的特征得分为 " << best.second
           << "，次高得分为 " << second;
    return CompetitionTypeResult{
        .type = best.first, .confidence = confidence, .reason = reason.str()};
}

} // namespace cc
