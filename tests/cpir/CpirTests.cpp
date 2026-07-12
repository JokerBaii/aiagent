#include "../TestSupport.hpp"

#include "cc/cpir/CPIRBuilder.hpp"
#include "cc/cpir/CompetitionTypeDetector.hpp"

namespace {

[[nodiscard]] cc::ProjectAsset material(std::string path, cc::AssetRole role) {
    cc::ProjectAsset asset;
    asset.relativePath = std::move(path);
    asset.role = role;
    asset.auditable = true;
    return asset;
}

} // namespace

void runCpirTests() {
    cc::ProjectInventory inventory;
    inventory.root = "input";
    inventory.assets = {
        material("README.md", cc::AssetRole::ProjectDeclaration),
        material("src/app.js", cc::AssetRole::SourceCode),
        material("review.md", cc::AssetRole::ProjectDeclaration),
    };
    const std::vector<cc::TextDocument> corpus = {
        {"README.md", "README", "# Demo\n目标用户：学生\n解决方案：可信审计\n商业模式：订阅\n",
         "EXTRACTED_TEXT"},
        {"src/app.js", "source", "// 市场规模：SAM 9999 亿元\n// 当前成果：用户名或密码错误\n",
         "EXTRACTED_TEXT"},
        {"review.md", "review", "目标用户：虚构用户\n", "NEED_REVIEW_TEXT_TRUNCATED"},
    };
    const auto explicitType = cc::CompetitionTypeDetector{}.detectDetailed(
        inventory, corpus, cc::CompetitionType::BusinessInnovation);
    const auto cpir = cc::CPIRBuilder{}.build(inventory, corpus, explicitType);
    requireTrue(cpir.projectName == "Demo", "CPIR project name mismatch");
    requireTrue(cpir.targetUser == "目标用户：学生", "CPIR should use trusted labeled fields");
    requireTrue(cpir.marketAnalysis.empty(), "source comments must not populate CPIR fields");
    requireTrue(cpir.currentResults.empty(), "incidental source messages must not become results");
    requireTrue(explicitType.confidence == 1.0, "explicit competition type should be trusted");
    requireTrue(!cpir.riskItems.empty(), "review-only text should be reflected as a CPIR risk");

    inventory.roleCounts[cc::AssetRole::BusinessPlan] = 1U;
    inventory.roleCounts[cc::AssetRole::FinancialPlan] = 1U;
    inventory.roleCounts[cc::AssetRole::MarketResearch] = 1U;
    inventory.roleCounts[cc::AssetRole::SocialPracticeProof] = 1U;
    const auto autoDetected = cc::CompetitionTypeDetector{}.detectDetailed(
        inventory, corpus, cc::CompetitionType::Unknown);
    requireTrue(autoDetected.type == cc::CompetitionType::BusinessInnovation,
                "strong business materials should take precedence over support proof roles");

    const std::vector<std::pair<std::string, cc::CompetitionType>> tracks = {
        {"商业模式：订阅", cc::CompetitionType::BusinessInnovation},
        {"软件系统：校园服务平台", cc::CompetitionType::SoftwareProject},
        {"硬件样机：传感器工程设计", cc::CompetitionType::EngineeringProduct},
        {"科学研究：对照实验与 baseline", cc::CompetitionType::ScientificResearch},
        {"社会实践：三下乡实地调研", cc::CompetitionType::SocialPractice},
        {"公益项目：面向弱势群体的志愿服务", cc::CompetitionType::PublicWelfare},
        {"三创电子商务平台", cc::CompetitionType::Ecommerce},
        {"人工智能大模型应用", cc::CompetitionType::AiApplication},
        {"挑战杯综合创新项目", cc::CompetitionType::ComprehensiveInnovation},
    };
    for (const auto& [text, expected] : tracks) {
        cc::ProjectInventory trackInventory;
        trackInventory.assets.push_back(material("申报书.md", cc::AssetRole::ProjectDeclaration));
        const std::vector<cc::TextDocument> trackCorpus = {
            {"申报书.md", "申报书", text, "EXTRACTED_TEXT"}};
        const auto detected = cc::CompetitionTypeDetector{}.detectDetailed(
            trackInventory, trackCorpus, cc::CompetitionType::Unknown);
        requireTrue(detected.type == expected,
                    "competition detector should cover every supported track type");
    }

    cc::ProjectInventory sourceOnly;
    sourceOnly.assets.push_back(material("src/app.js", cc::AssetRole::SourceCode));
    sourceOnly.roleCounts[cc::AssetRole::SourceCode] = 1U;
    const std::vector<cc::TextDocument> injected = {
        {"src/app.js", "source", "三创 电商 人工智能 公益项目", "EXTRACTED_TEXT"}};
    const auto sourceDetected = cc::CompetitionTypeDetector{}.detectDetailed(
        sourceOnly, injected, cc::CompetitionType::Unknown);
    requireTrue(sourceDetected.type == cc::CompetitionType::Unknown,
                "source text must not inject a competition track");
}
