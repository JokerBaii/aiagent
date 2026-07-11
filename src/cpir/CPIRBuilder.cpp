#include "cc/cpir/CPIRBuilder.hpp"

#include "cc/core/Enums.hpp"
#include "cc/inventory/MaterialTrustPolicy.hpp"
#include "cc/util/StringUtil.hpp"

#include <optional>

namespace cc {
namespace {

[[nodiscard]] bool containsAny(const std::string& text,
                               std::initializer_list<std::string_view> values) {
    for (const auto value : values) {
        if (util::contains(text, value)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool negativeOrEmptyValue(const std::string& line) {
    const auto lower = util::lowerAscii(line);
    return containsAny(lower, {"暂无", "尚未", "暂未", "没有", "未确定", "待确定", "待补充",
                               "不涉及", "unknown", "n/a", "todo", "tbd"});
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

[[nodiscard]] std::string stripMarkdownPrefix(std::string line) {
    line = util::trim(std::move(line));
    while (!line.empty() &&
           (line.front() == '#' || line.front() == '-' || line.front() == '*' ||
            line.front() == '>')) {
        line = util::trim(line.substr(1U));
    }
    return line;
}

[[nodiscard]] std::string fieldLine(const ProjectInventory& inventory,
                                    const std::vector<TextDocument>& corpus,
                                    std::initializer_list<std::string_view> labels) {
    for (const auto& document : corpus) {
        if (!trustedDocument(document, inventory)) {
            continue;
        }
        for (const auto& rawLine : util::splitLines(document.text)) {
            const auto line = stripMarkdownPrefix(rawLine);
            if (line.size() < 4U || line.size() > 300U || negativeOrEmptyValue(line)) {
                continue;
            }
            const auto lower = util::lowerAscii(line);
            for (const auto label : labels) {
                const auto normalizedLabel = util::lowerAscii(std::string{label});
                const auto position = lower.find(normalizedLabel);
                if (position == std::string::npos || position > 12U) {
                    continue;
                }
                const auto valueAt = position + normalizedLabel.size();
                if (valueAt >= lower.size()) {
                    continue;
                }
                const auto remainder = util::trim(line.substr(valueAt));
                if (remainder.empty() ||
                    (!remainder.starts_with(":") && !remainder.starts_with("：") &&
                     !remainder.starts_with("是") && !remainder.starts_with("为") &&
                     !remainder.starts_with("-") && !remainder.starts_with("—") &&
                     position != 0U)) {
                    continue;
                }
                return line;
            }
        }
    }
    return {};
}

[[nodiscard]] std::string firstMarkdownTitle(const ProjectInventory& inventory,
                                             const std::vector<TextDocument>& corpus) {
    for (const auto& document : corpus) {
        if (!trustedDocument(document, inventory)) {
            continue;
        }
        for (const auto& rawLine : util::splitLines(document.text)) {
            const auto line = util::trim(rawLine);
            if (line.rfind("# ", 0U) == 0U && line.size() > 2U && line.size() <= 120U) {
                return util::trim(line.substr(2U));
            }
        }
    }
    return {};
}

[[nodiscard]] std::string projectNameFallback(const ProjectInventory& inventory) {
    for (const auto& asset : inventory.assets) {
        if (asset.role != AssetRole::ProjectDeclaration && asset.role != AssetRole::BusinessPlan &&
            asset.role != AssetRole::PitchDeck) {
            continue;
        }
        const auto stem = asset.relativePath.stem().string();
        const auto lower = util::lowerAscii(stem);
        if (!stem.empty() && lower != "readme" && lower != "project" && lower != "overview") {
            return stem;
        }
    }

    std::optional<std::string> topLevel;
    bool commonTopLevel = true;
    for (const auto& asset : inventory.assets) {
        if (asset.relativePath.parent_path().empty()) {
            commonTopLevel = false;
            break;
        }
        const auto first = asset.relativePath.begin()->string();
        if (!topLevel.has_value()) {
            topLevel = first;
        } else if (*topLevel != first) {
            commonTopLevel = false;
            break;
        }
    }
    return commonTopLevel && topLevel.has_value() ? *topLevel : "未命名项目";
}

} // namespace

CPIR CPIRBuilder::build(const ProjectInventory& inventory, const std::vector<TextDocument>& corpus,
                        const CompetitionTypeResult& type) const {
    CPIR cpir;
    cpir.projectName = firstMarkdownTitle(inventory, corpus);
    if (cpir.projectName.empty()) {
        cpir.projectName = projectNameFallback(inventory);
    }
    cpir.competitionType = type.type;
    cpir.competitionConfidence = type.confidence;
    cpir.competitionReason = type.reason;
    cpir.track = trackKey(type.type);
    cpir.targetUser = fieldLine(inventory, corpus, {"目标用户", "用户画像", "服务对象", "target_user"});
    cpir.painPoint = fieldLine(inventory, corpus, {"用户痛点", "核心痛点", "实际问题", "pain_point"});
    cpir.solution = fieldLine(inventory, corpus, {"解决方案", "解决路径", "solution"});
    cpir.productOrService =
        fieldLine(inventory, corpus, {"产品或服务", "核心产品", "产品形态", "product_service"});
    cpir.technicalRoute = fieldLine(inventory, corpus, {"技术路线", "技术架构", "核心算法", "technical_route"});
    cpir.businessModel = fieldLine(inventory, corpus, {"商业模式", "收入模式", "付费方", "business_model"});
    cpir.marketAnalysis =
        fieldLine(inventory, corpus, {"市场规模", "市场分析", "tam", "sam", "som", "market_analysis"});
    cpir.competitorAnalysis =
        fieldLine(inventory, corpus, {"竞品分析", "竞争分析", "差异化", "competitor_analysis"});
    cpir.financialProjection =
        fieldLine(inventory, corpus, {"财务预测", "营收预测", "成本结构", "financial_projection"});
    cpir.teamStructure = fieldLine(inventory, corpus, {"团队结构", "团队分工", "核心成员", "team_structure"});
    cpir.currentResults = fieldLine(inventory, corpus, {"当前成果", "已取得成果", "项目进展", "current_results"});
    cpir.socialValue = fieldLine(inventory, corpus, {"社会价值", "社会影响", "公益价值", "social_value"});

    const std::vector<std::pair<std::string, std::string>> required = {
        {"target_user", cpir.targetUser},
        {"solution", cpir.solution},
        {"business_model", cpir.businessModel},
    };
    for (const auto& [field, value] : required) {
        if (value.empty()) {
            cpir.missingFields.push_back(field);
            cpir.riskItems.push_back("CPIR 字段缺失: " + field);
        }
    }
    std::size_t reviewCount = 0U;
    for (const auto& document : corpus) {
        if (needsTextReview(document) && !isExcludedTruthPath(document.sourceFile)) {
            ++reviewCount;
        }
    }
    if (reviewCount > 0U) {
        cpir.riskItems.push_back("有 " + std::to_string(reviewCount) +
                                 " 份材料的文本不完整或无法可靠抽取，未用于项目画像。");
    }
    if (type.type == CompetitionType::Unknown) {
        cpir.riskItems.push_back("材料特征不足，赛道需要人工确认。");
    }
    return cpir;
}

} // namespace cc
