#include "../TestSupport.hpp"

#include "cc/claim/ClaimExtractor.hpp"

namespace {

[[nodiscard]] cc::ProjectAsset narrative(std::string path, cc::AssetRole role) {
    cc::ProjectAsset asset;
    asset.relativePath = std::move(path);
    asset.role = role;
    asset.auditable = true;
    return asset;
}

} // namespace

void runClaimTests() {
    cc::ProjectInventory inventory;
    inventory.assets = {
        narrative("申报书.md", cc::AssetRole::ProjectDeclaration),
        narrative("商业计划.md", cc::AssetRole::BusinessPlan),
        narrative("src/app.js", cc::AssetRole::SourceCode),
        narrative("mocks/fake.md", cc::AssetRole::ProjectDeclaration),
        narrative("待复核.md", cc::AssetRole::ProjectDeclaration),
    };
    const std::vector<cc::TextDocument> corpus = {
        {"申报书.md", "申报书",
         "市场规模：预计 TAM 达到 100 亿元\n"
         "市场规模：预计 TAM 达到 100 亿元\n"
         "未申请专利，禁止营收宣传\n"
         "与华星公司已签署合作协议\n",
         "EXTRACTED_TEXT"},
        {"商业计划.md", "商业计划", "市场规模：预计 TAM 达到 100 亿元\n",
         "EXTRACTED_TEXT"},
        {"src/app.js", "source", "已有用户达到 99999 人\n", "EXTRACTED_TEXT"},
        {"mocks/fake.md", "mock", "已获得专利，专利号 12345678\n", "EXTRACTED_TEXT"},
        {"待复核.md", "review", "已获得专利，专利号 87654321\n",
         "NEED_REVIEW_TEXT_TRUNCATED"},
    };

    const auto claims = cc::ClaimExtractor{}.extract(corpus, inventory);
    requireTrue(claims.size() == 2U,
                "claims should be deduplicated and exclude negated/untrusted/review text");
    requireTrue(claims[0].claimType == cc::ClaimType::MarketScale,
                "market-scale claim should be extracted");
    requireTrue(claims[1].claimType == cc::ClaimType::Partnership,
                "explicit signed partnership should be extracted");
    requireTrue(claims[0].claimId == "CLM-001" && claims[1].claimId == "CLM-002",
                "claim identifiers should remain deterministic after deduplication");
}
