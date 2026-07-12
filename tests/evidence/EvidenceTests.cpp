#include "../TestSupport.hpp"

#include "cc/evidence/EvidenceGraph.hpp"
#include "cc/evidence/EvidenceMatcher.hpp"

namespace {

[[nodiscard]] cc::ProjectAsset evidenceAsset(std::string path, cc::AssetRole role) {
    cc::ProjectAsset asset;
    asset.relativePath = std::move(path);
    asset.role = role;
    asset.auditable = true;
    return asset;
}

} // namespace

void runEvidenceTests() {
    const cc::ProjectClaim marketClaim{
        "CLM-001", "市场规模预计达到 100 亿元", cc::ClaimType::MarketScale, "plan.md", 0.8, ""};
    cc::ProjectInventory marketInventory;
    marketInventory.assets.push_back(evidenceAsset("市场调研.md", cc::AssetRole::MarketResearch));

    const auto roleOnly = cc::EvidenceMatcher{}.match({marketClaim}, marketInventory);
    requireTrue(roleOnly.front().status == cc::EvidenceStatus::NeedReview,
                "asset role alone must never prove a claim");

    const std::vector<cc::TextDocument> unrelated = {
        {"市场调研.md", "市场调研", "数据来源：行业年鉴，市场规模达到 200 亿元", "EXTRACTED_TEXT"}};
    const auto mismatched = cc::EvidenceMatcher{}.match({marketClaim}, marketInventory, unrelated);
    requireTrue(mismatched.front().status == cc::EvidenceStatus::Unsupported,
                "evidence with a different key value must not support the claim");

    const std::vector<cc::TextDocument> supportedCorpus = {
        {"市场调研.md", "市场调研", "数据来源：行业年鉴，市场规模达到 100 亿元", "EXTRACTED_TEXT"}};
    const auto supported =
        cc::EvidenceMatcher{}.match({marketClaim}, marketInventory, supportedCorpus);
    requireTrue(supported.front().status == cc::EvidenceStatus::Supported,
                "matching sourced market data should support the same numeric claim");
    cc::EvidenceGraph graph{supported};
    requireTrue(graph.findByClaimId("CLM-001").has_value(), "evidence graph should find claim");
    requireTrue(graph.coveragePercent() == 100.0,
                "supported evidence should count toward coverage");

    const std::vector<cc::TextDocument> truncated = {
        {"市场调研.md", "市场调研", "市场规模达到 100 亿元", "NEED_REVIEW_TEXT_TRUNCATED"}};
    const auto review = cc::EvidenceMatcher{}.match({marketClaim}, marketInventory, truncated);
    requireTrue(review.front().status == cc::EvidenceStatus::NeedReview,
                "truncated evidence must not receive a high-confidence status");

    const cc::ProjectClaim revenueClaim{
        "CLM-002", "已实现营收 50 万元", cc::ClaimType::Revenue, "plan.md", 0.8, ""};
    cc::ProjectInventory revenueInventory;
    revenueInventory.assets = {
        evidenceAsset("财务预测.md", cc::AssetRole::FinancialPlan),
        evidenceAsset("订单证明.md", cc::AssetRole::ProofMaterial),
    };
    const std::vector<cc::TextDocument> financialOnly = {
        {"财务预测.md", "财务预测", "预计营收 50 万元", "EXTRACTED_TEXT"}};
    const auto partial =
        cc::EvidenceMatcher{}.match({revenueClaim}, revenueInventory, financialOnly);
    requireTrue(partial.front().status == cc::EvidenceStatus::Partial,
                "a financial projection is related but is not independent revenue proof");
    const std::vector<cc::TextDocument> orderProof = {
        {"财务预测.md", "财务预测", "预计营收 50 万元", "EXTRACTED_TEXT"},
        {"订单证明.md", "订单证明", "订单编号 A-100，发票号 F-50，实现收入 50 万元",
         "EXTRACTED_TEXT"},
    };
    const auto revenueSupported =
        cc::EvidenceMatcher{}.match({revenueClaim}, revenueInventory, orderProof);
    requireTrue(revenueSupported.front().status == cc::EvidenceStatus::Supported,
                "matching traceable order material should support a revenue claim");

    const cc::ProjectClaim patentClaim{
        "CLM-003", "已获得专利，专利号 12345678", cc::ClaimType::Patent, "plan.md", 0.8, ""};
    cc::ProjectInventory patentInventory;
    patentInventory.assets.push_back(evidenceAsset("专利说明.md", cc::AssetRole::PatentCopyright));
    const std::vector<cc::TextDocument> contradiction = {
        {"专利说明.md", "专利说明", "专利号 12345678 当前未授权", "EXTRACTED_TEXT"}};
    const auto conflicted =
        cc::EvidenceMatcher{}.match({patentClaim}, patentInventory, contradiction);
    requireTrue(conflicted.front().status == cc::EvidenceStatus::Conflicted,
                "explicit contradictory proof should produce a conflict");
}
