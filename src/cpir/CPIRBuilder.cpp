/**
 * @file CPIRBuilder.cpp
 * @brief CPIR 项目中间表示构建实现。
 */

#include "cc/cpir/CPIRBuilder.hpp"
#include "cc/core/Enums.hpp"
#include "cc/util/StringUtil.hpp"

namespace cc {
namespace {

[[nodiscard]] std::string firstLineContaining(const std::vector<TextDocument>& corpus,
                                              const std::vector<std::string>& keywords) {
    for (const auto& doc : corpus) {
        for (const auto& rawLine : util::splitLines(doc.text)) {
            const auto line = util::trim(rawLine);
            if (line.empty() || line.size() > 300U) {
                continue;
            }
            for (const auto& keyword : keywords) {
                if (util::contains(line, keyword) ||
                    util::contains(util::lowerAscii(line), util::lowerAscii(keyword))) {
                    return line;
                }
            }
        }
    }
    return {};
}

[[nodiscard]] std::string firstMarkdownTitle(const std::vector<TextDocument>& corpus) {
    for (const auto& doc : corpus) {
        for (const auto& rawLine : util::splitLines(doc.text)) {
            const auto line = util::trim(rawLine);
            if (line.rfind("# ", 0) == 0 && line.size() > 2U) {
                return util::trim(line.substr(2));
            }
        }
    }
    return {};
}

} // namespace

CPIR CPIRBuilder::build(const ProjectInventory& inventory, const std::vector<TextDocument>& corpus,
                        const CompetitionTypeResult& type) const {
    CPIR cpir;
    cpir.projectName = firstMarkdownTitle(corpus);
    if (cpir.projectName.empty()) {
        cpir.projectName = inventory.root.filename().string();
    }
    cpir.competitionType = type.type;
    cpir.competitionConfidence = type.confidence;
    cpir.competitionReason = type.reason;
    cpir.track = trackKey(type.type);
    cpir.targetUser = firstLineContaining(corpus, {"目标用户", "用户画像", "服务对象"});
    cpir.painPoint = firstLineContaining(corpus, {"痛点", "问题", "需求"});
    cpir.solution = firstLineContaining(corpus, {"解决方案", "方案", "产品"});
    cpir.productOrService = firstLineContaining(corpus, {"产品", "服务", "平台", "系统"});
    cpir.technicalRoute = firstLineContaining(corpus, {"技术路线", "架构", "算法", "模型"});
    cpir.businessModel = firstLineContaining(corpus, {"商业模式", "收入", "付费", "渠道"});
    cpir.marketAnalysis =
        firstLineContaining(corpus, {"市场规模", "市场分析", "TAM", "SAM", "SOM"});
    cpir.competitorAnalysis = firstLineContaining(corpus, {"竞品", "竞争", "差异化"});
    cpir.financialProjection = firstLineContaining(corpus, {"财务", "成本", "营收", "利润"});
    cpir.teamStructure = firstLineContaining(corpus, {"团队", "成员", "分工"});
    cpir.currentResults = firstLineContaining(corpus, {"成果", "用户", "订单", "专利", "软著"});
    cpir.socialValue = firstLineContaining(corpus, {"社会价值", "社会影响", "公益", "服务"});

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
    return cpir;
}

CPIR CPIRBuilder::build(const ProjectInventory& inventory, const std::vector<TextDocument>& corpus,
                        CompetitionType type) const {
    return build(inventory, corpus,
                 CompetitionTypeResult{.type = type,
                                       .confidence = type == CompetitionType::Unknown ? 0.0 : 1.0,
                                       .reason = "调用方提供了赛道类型"});
}

} // namespace cc
