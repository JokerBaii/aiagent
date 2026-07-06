/**
 * @file ConsistencyChecker.cpp
 * @brief 材料一致性风险检查实现。
 */

#include "cc/consistency/ConsistencyChecker.hpp"
#include "cc/inventory/InventoryEngine.hpp"

#include <algorithm>

namespace cc {

std::vector<ConsistencyIssue>
ConsistencyChecker::check(const CPIR& cpir, const ProjectInventory& inventory,
                          const std::vector<ProjectClaim>& claims) const {
    std::vector<ConsistencyIssue> issues;
    auto add = [&](std::string id, Severity severity, std::string description,
                   std::vector<std::filesystem::path> files, std::string suggestion) {
        ConsistencyIssue issue;
        issue.issueId = std::move(id);
        issue.severity = severity;
        issue.description = std::move(description);
        issue.affectedFiles = std::move(files);
        issue.fixSuggestion = std::move(suggestion);
        issues.push_back(std::move(issue));
    };

    if (hasRole(inventory, AssetRole::SourceCode) && !hasRole(inventory, AssetRole::BuildSystem) &&
        !hasRole(inventory, AssetRole::DependencyManifest)) {
        add("CONSISTENCY_SOFT_001", Severity::Warning, "项目包含源码，但缺少构建入口或依赖清单。",
            filesWithRole(inventory, AssetRole::SourceCode),
            "补充 CMakeLists.txt、package.json、requirements.txt 或等价构建说明。");
    }
    if (hasRole(inventory, AssetRole::FinancialPlan) &&
        !hasRole(inventory, AssetRole::BusinessPlan)) {
        add("CONSISTENCY_BIZ_001", Severity::Warning,
            "存在财务预测材料，但缺少商业计划书解释收入、成本和付费方。",
            filesWithRole(inventory, AssetRole::FinancialPlan), "补充商业计划书或商业模式画布。");
    }
    if (!cpir.marketAnalysis.empty() && cpir.targetUser.empty()) {
        add("CONSISTENCY_MARKET_001", Severity::Warning,
            "材料出现市场分析，但未抽取到目标用户或用户画像。",
            filesWithRole(inventory, AssetRole::MarketResearch),
            "在申报书或商业计划书中明确目标用户和样本来源。");
    }

    const auto hasRevenueClaim =
        std::any_of(claims.begin(), claims.end(), [](const ProjectClaim& claim) {
            return claim.claimType == ClaimType::Revenue;
        });
    if (hasRevenueClaim && !hasRole(inventory, AssetRole::FinancialPlan) &&
        !hasRole(inventory, AssetRole::ProofMaterial)) {
        add("CONSISTENCY_REVENUE_001", Severity::Warning,
            "材料存在收入或订单声明，但缺少财务预测或交易证明。", {},
            "补充订单、合同、流水或收入测算表。");
    }
    return issues;
}

} // namespace cc
