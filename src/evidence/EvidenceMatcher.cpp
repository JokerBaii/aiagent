/**
 * @file EvidenceMatcher.cpp
 * @brief 声明与证据材料匹配实现。
 */

#include "cc/evidence/EvidenceMatcher.hpp"

#include <algorithm>

namespace cc {
namespace {

[[nodiscard]] std::vector<AssetRole> evidenceRolesForClaim(ClaimType type) {
    switch (type) {
    case ClaimType::UserTraction:
        return {AssetRole::UserResearch, AssetRole::ProofMaterial, AssetRole::MarketResearch};
    case ClaimType::MarketScale:
        return {AssetRole::MarketResearch};
    case ClaimType::TechnicalCapability:
        return {AssetRole::SourceCode, AssetRole::BuildSystem, AssetRole::DeploymentDoc};
    case ClaimType::BusinessModel:
        return {AssetRole::BusinessPlan, AssetRole::FinancialPlan};
    case ClaimType::Revenue:
        return {AssetRole::FinancialPlan, AssetRole::ProofMaterial};
    case ClaimType::Patent:
    case ClaimType::Copyright:
        return {AssetRole::PatentCopyright, AssetRole::ProofMaterial};
    case ClaimType::Partnership:
        return {AssetRole::ProofMaterial, AssetRole::SocialPracticeProof};
    case ClaimType::ResearchResult:
        return {AssetRole::ExperimentData, AssetRole::ResearchPaper};
    case ClaimType::SocialImpact:
        return {AssetRole::SocialPracticeProof, AssetRole::UserResearch};
    case ClaimType::Deployment:
        return {AssetRole::DeploymentDoc, AssetRole::SourceCode};
    case ClaimType::CostReduction:
    case ClaimType::Prototype:
    case ClaimType::Unknown:
        return {AssetRole::ProofMaterial};
    }
    return {AssetRole::ProofMaterial};
}

} // namespace

std::vector<std::string> missingEvidenceForClaim(ClaimType type) {
    switch (type) {
    case ClaimType::UserTraction:
        return {"用户数据", "后台截图", "问卷样本", "访谈记录"};
    case ClaimType::MarketScale:
        return {"行业报告", "统计数据", "TAM/SAM/SOM 拆解"};
    case ClaimType::TechnicalCapability:
        return {"源码", "构建入口", "部署说明"};
    case ClaimType::BusinessModel:
        return {"商业模式画布", "财务预测假设", "成本结构"};
    case ClaimType::Revenue:
        return {"订单", "合同", "流水", "收入测算表"};
    case ClaimType::Patent:
        return {"专利申请受理通知书"};
    case ClaimType::Copyright:
        return {"软著证书或受理材料"};
    case ClaimType::Partnership:
        return {"合作协议", "盖章证明", "邮件记录"};
    case ClaimType::ResearchResult:
        return {"实验数据", "评价指标", "baseline"};
    case ClaimType::SocialImpact:
        return {"服务对象证明", "活动记录", "影响数据"};
    case ClaimType::Deployment:
        return {"部署说明", "运行截图"};
    case ClaimType::CostReduction:
    case ClaimType::Prototype:
    case ClaimType::Unknown:
        return {"可追溯证明材料"};
    }
    return {"可追溯证明材料"};
}

std::vector<EvidenceMatch> EvidenceMatcher::match(const std::vector<ProjectClaim>& claims,
                                                  const ProjectInventory& inventory) const {
    std::vector<EvidenceMatch> matches;
    for (const auto& claim : claims) {
        const auto roles = evidenceRolesForClaim(claim.claimType);
        EvidenceMatch match;
        match.claimId = claim.claimId;
        match.missingEvidence = missingEvidenceForClaim(claim.claimType);

        bool independent = false;
        for (const auto& asset : inventory.assets) {
            const bool roleMatched =
                std::find(roles.begin(), roles.end(), asset.role) != roles.end();
            if (!roleMatched || asset.sensitive || asset.generated || asset.vendored) {
                continue;
            }
            match.evidenceFiles.push_back(asset.relativePath);
            independent = independent || asset.relativePath != claim.sourceFile;
        }

        if (match.evidenceFiles.empty()) {
            match.status = EvidenceStatus::Unsupported;
            match.reason = "未发现可映射到该声明类型的证据材料。";
        } else if (independent) {
            match.status = EvidenceStatus::Supported;
            match.reason = "发现独立证据材料可支撑该声明。";
            match.missingEvidence.clear();
        } else {
            match.status = EvidenceStatus::Partial;
            match.reason = "仅发现声明所在材料，缺少独立证明材料。";
        }
        matches.push_back(std::move(match));
    }
    return matches;
}

} // namespace cc
