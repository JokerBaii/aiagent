/**
 * @file Enums.cpp
 * @brief 核心枚举和稳定字符串之间的转换。
 */

#include "cc/core/Enums.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace cc {
namespace {

[[nodiscard]] std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace

std::string toString(AssetRole role) {
    switch (role) {
    case AssetRole::ProjectDeclaration:
        return "PROJECT_DECLARATION";
    case AssetRole::BusinessPlan:
        return "BUSINESS_PLAN";
    case AssetRole::PitchDeck:
        return "PITCH_DECK";
    case AssetRole::MarketResearch:
        return "MARKET_RESEARCH";
    case AssetRole::CompetitorAnalysis:
        return "COMPETITOR_ANALYSIS";
    case AssetRole::FinancialPlan:
        return "FINANCIAL_PLAN";
    case AssetRole::UserResearch:
        return "USER_RESEARCH";
    case AssetRole::SourceCode:
        return "SOURCE_CODE";
    case AssetRole::BuildSystem:
        return "BUILD_SYSTEM";
    case AssetRole::DependencyManifest:
        return "DEPENDENCY_MANIFEST";
    case AssetRole::DeploymentDoc:
        return "DEPLOYMENT_DOC";
    case AssetRole::ExperimentData:
        return "EXPERIMENT_DATA";
    case AssetRole::ResearchPaper:
        return "RESEARCH_PAPER";
    case AssetRole::PatentCopyright:
        return "PATENT_COPYRIGHT";
    case AssetRole::ProofMaterial:
        return "PROOF_MATERIAL";
    case AssetRole::SocialPracticeProof:
        return "SOCIAL_PRACTICE_PROOF";
    case AssetRole::ResourceAsset:
        return "RESOURCE_ASSET";
    case AssetRole::ModelArtifact:
        return "MODEL_ARTIFACT";
    case AssetRole::BinaryArtifact:
        return "BINARY_ARTIFACT";
    case AssetRole::Archive:
        return "ARCHIVE";
    case AssetRole::Generated:
        return "GENERATED";
    case AssetRole::Vendored:
        return "VENDORED";
    case AssetRole::SecretRisk:
        return "SECRET_RISK";
    case AssetRole::Unknown:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::string toString(Severity severity) {
    switch (severity) {
    case Severity::Info:
        return "INFO";
    case Severity::Warning:
        return "WARNING";
    case Severity::Blocker:
        return "BLOCKER";
    }
    return "WARNING";
}

std::string toString(CompetitionType type) {
    switch (type) {
    case CompetitionType::BusinessInnovation:
        return "商业创新";
    case CompetitionType::SoftwareProject:
        return "软件开发";
    case CompetitionType::EngineeringProduct:
        return "工程产品";
    case CompetitionType::ScientificResearch:
        return "科研学术";
    case CompetitionType::SocialPractice:
        return "社会实践";
    case CompetitionType::PublicWelfare:
        return "公益创业";
    case CompetitionType::Ecommerce:
        return "电商三创";
    case CompetitionType::AiApplication:
        return "AI 应用";
    case CompetitionType::ComprehensiveInnovation:
        return "综合创新创业";
    case CompetitionType::Unknown:
        return "未知";
    }
    return "未知";
}

std::string toString(ClaimType type) {
    switch (type) {
    case ClaimType::UserTraction:
        return "UserTraction";
    case ClaimType::MarketScale:
        return "MarketScale";
    case ClaimType::TechnicalCapability:
        return "TechnicalCapability";
    case ClaimType::BusinessModel:
        return "BusinessModel";
    case ClaimType::Revenue:
        return "Revenue";
    case ClaimType::CostReduction:
        return "CostReduction";
    case ClaimType::Patent:
        return "Patent";
    case ClaimType::Copyright:
        return "Copyright";
    case ClaimType::Partnership:
        return "Partnership";
    case ClaimType::Prototype:
        return "Prototype";
    case ClaimType::ResearchResult:
        return "ResearchResult";
    case ClaimType::SocialImpact:
        return "SocialImpact";
    case ClaimType::Deployment:
        return "Deployment";
    case ClaimType::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string toString(EvidenceStatus status) {
    switch (status) {
    case EvidenceStatus::Supported:
        return "SUPPORTED";
    case EvidenceStatus::Partial:
        return "PARTIAL";
    case EvidenceStatus::Unsupported:
        return "UNSUPPORTED";
    case EvidenceStatus::Conflicted:
        return "CONFLICTED";
    case EvidenceStatus::NeedReview:
        return "NEED_REVIEW";
    }
    return "UNSUPPORTED";
}

std::string toString(ToolPermission permission) {
    switch (permission) {
    case ToolPermission::ReadProjectFiles:
        return "ReadProjectFiles";
    case ToolPermission::ReadExternalFiles:
        return "ReadExternalFiles";
    case ToolPermission::WriteWorkspace:
        return "WriteWorkspace";
    case ToolPermission::ModifyOriginalProject:
        return "ModifyOriginalProject";
    case ToolPermission::ExecuteCommand:
        return "ExecuteCommand";
    case ToolPermission::NetworkAccess:
        return "NetworkAccess";
    case ToolPermission::LLMAccess:
        return "LLMAccess";
    case ToolPermission::ExportReport:
        return "ExportReport";
    }
    return "Unknown";
}

std::string toString(HookPoint point) {
    switch (point) {
    case HookPoint::BeforeProjectLoad:
        return "BeforeProjectLoad";
    case HookPoint::AfterProjectLoad:
        return "AfterProjectLoad";
    case HookPoint::BeforeInventory:
        return "BeforeInventory";
    case HookPoint::AfterInventory:
        return "AfterInventory";
    case HookPoint::BeforeAudit:
        return "BeforeAudit";
    case HookPoint::AfterAudit:
        return "AfterAudit";
    case HookPoint::BeforeRepairPlan:
        return "BeforeRepairPlan";
    case HookPoint::AfterRepairPlan:
        return "AfterRepairPlan";
    case HookPoint::BeforeReportExport:
        return "BeforeReportExport";
    case HookPoint::AfterReportExport:
        return "AfterReportExport";
    }
    return "Unknown";
}

AssetRole assetRoleFromString(const std::string& value) {
    static const std::unordered_map<std::string, AssetRole> roles = {
        {"PROJECT_DECLARATION", AssetRole::ProjectDeclaration},
        {"BUSINESS_PLAN", AssetRole::BusinessPlan},
        {"PITCH_DECK", AssetRole::PitchDeck},
        {"MARKET_RESEARCH", AssetRole::MarketResearch},
        {"COMPETITOR_ANALYSIS", AssetRole::CompetitorAnalysis},
        {"FINANCIAL_PLAN", AssetRole::FinancialPlan},
        {"USER_RESEARCH", AssetRole::UserResearch},
        {"SOURCE_CODE", AssetRole::SourceCode},
        {"BUILD_SYSTEM", AssetRole::BuildSystem},
        {"DEPENDENCY_MANIFEST", AssetRole::DependencyManifest},
        {"DEPLOYMENT_DOC", AssetRole::DeploymentDoc},
        {"EXPERIMENT_DATA", AssetRole::ExperimentData},
        {"RESEARCH_PAPER", AssetRole::ResearchPaper},
        {"PATENT_COPYRIGHT", AssetRole::PatentCopyright},
        {"PROOF_MATERIAL", AssetRole::ProofMaterial},
        {"SOCIAL_PRACTICE_PROOF", AssetRole::SocialPracticeProof},
        {"RESOURCE_ASSET", AssetRole::ResourceAsset},
        {"MODEL_ARTIFACT", AssetRole::ModelArtifact},
        {"BINARY_ARTIFACT", AssetRole::BinaryArtifact},
        {"ARCHIVE", AssetRole::Archive},
        {"GENERATED", AssetRole::Generated},
        {"VENDORED", AssetRole::Vendored},
        {"SECRET_RISK", AssetRole::SecretRisk},
        {"UNKNOWN", AssetRole::Unknown},
    };
    const auto iter = roles.find(value);
    return iter == roles.end() ? AssetRole::Unknown : iter->second;
}

Severity severityFromString(const std::string& value) {
    const auto lower = lowerAscii(value);
    if (lower == "blocker") {
        return Severity::Blocker;
    }
    if (lower == "info") {
        return Severity::Info;
    }
    return Severity::Warning;
}

CompetitionType competitionTypeFromTrack(const std::string& value) {
    const auto lower = lowerAscii(value);
    if (lower == "business" || lower == "business_innovation" || lower == "biz") {
        return CompetitionType::BusinessInnovation;
    }
    if (lower == "software" || lower == "software_project" || lower == "soft") {
        return CompetitionType::SoftwareProject;
    }
    if (lower == "research" || lower == "scientific_research") {
        return CompetitionType::ScientificResearch;
    }
    if (lower == "social" || lower == "social_practice") {
        return CompetitionType::SocialPractice;
    }
    if (lower == "ecommerce" || lower == "e-commerce") {
        return CompetitionType::Ecommerce;
    }
    if (lower == "ai" || lower == "ai_application") {
        return CompetitionType::AiApplication;
    }
    if (lower == "engineering" || lower == "engineering_product") {
        return CompetitionType::EngineeringProduct;
    }
    return CompetitionType::Unknown;
}

std::string trackKey(CompetitionType type) {
    switch (type) {
    case CompetitionType::BusinessInnovation:
        return "business_innovation";
    case CompetitionType::SoftwareProject:
        return "software_project";
    case CompetitionType::EngineeringProduct:
        return "engineering_product";
    case CompetitionType::ScientificResearch:
        return "scientific_research";
    case CompetitionType::SocialPractice:
        return "social_practice";
    case CompetitionType::PublicWelfare:
        return "public_welfare";
    case CompetitionType::Ecommerce:
        return "ecommerce";
    case CompetitionType::AiApplication:
        return "ai_application";
    case CompetitionType::ComprehensiveInnovation:
        return "comprehensive_innovation";
    case CompetitionType::Unknown:
        return "unknown";
    }
    return "unknown";
}

} // namespace cc
