/**
 * @file Enums.hpp
 * @brief 核心枚举和字符串转换。
 */

#pragma once

#include <string>

namespace cc {

enum class AssetRole {
    ProjectDeclaration,
    BusinessPlan,
    PitchDeck,
    MarketResearch,
    CompetitorAnalysis,
    FinancialPlan,
    UserResearch,
    SourceCode,
    BuildSystem,
    DependencyManifest,
    DeploymentDoc,
    ExperimentData,
    ResearchPaper,
    PatentCopyright,
    ProofMaterial,
    SocialPracticeProof,
    ResourceAsset,
    ModelArtifact,
    BinaryArtifact,
    Archive,
    Generated,
    Vendored,
    SecretRisk,
    Unknown
};

enum class Severity { Info, Warning, Blocker };

enum class CompetitionType {
    BusinessInnovation,
    SoftwareProject,
    EngineeringProduct,
    ScientificResearch,
    SocialPractice,
    PublicWelfare,
    Ecommerce,
    AiApplication,
    ComprehensiveInnovation,
    Unknown
};

enum class ClaimType {
    UserTraction,
    MarketScale,
    TechnicalCapability,
    BusinessModel,
    Revenue,
    CostReduction,
    Patent,
    Copyright,
    Partnership,
    Prototype,
    ResearchResult,
    SocialImpact,
    Deployment,
    Unknown
};

enum class EvidenceStatus { Supported, Partial, Unsupported, Conflicted, NeedReview };

enum class ToolPermission {
    ReadProjectFiles,
    ReadExternalFiles,
    WriteWorkspace,
    ModifyOriginalProject,
    ExecuteCommand,
    NetworkAccess,
    LLMAccess,
    ExportReport
};

enum class HookPoint {
    BeforeProjectLoad,
    AfterProjectLoad,
    BeforeInventory,
    AfterInventory,
    BeforeAudit,
    AfterAudit,
    BeforeRepairPlan,
    AfterRepairPlan,
    BeforeReportExport,
    AfterReportExport
};

/** @brief 将资产角色转为中文/稳定字符串。 */
[[nodiscard]] std::string toString(AssetRole role);
/** @brief 将严重度转为稳定字符串。 */
[[nodiscard]] std::string toString(Severity severity);
/** @brief 将竞赛类型转为中文字符串。 */
[[nodiscard]] std::string toString(CompetitionType type);
/** @brief 将声明类型转为稳定字符串。 */
[[nodiscard]] std::string toString(ClaimType type);
/** @brief 将证据状态转为稳定字符串。 */
[[nodiscard]] std::string toString(EvidenceStatus status);
/** @brief 将工具权限转为稳定字符串。 */
[[nodiscard]] std::string toString(ToolPermission permission);
/** @brief 将 Hook 点转为稳定字符串。 */
[[nodiscard]] std::string toString(HookPoint point);
/** @brief 从规则包字符串解析资产角色。 */
[[nodiscard]] AssetRole assetRoleFromString(const std::string& value);
/** @brief 从规则包字符串解析严重度。 */
[[nodiscard]] Severity severityFromString(const std::string& value);
/** @brief 从用户选择的赛道文本解析竞赛类型。 */
[[nodiscard]] CompetitionType competitionTypeFromTrack(const std::string& value);
/** @brief 返回规则包使用的赛道 key。 */
[[nodiscard]] std::string trackKey(CompetitionType type);

} // namespace cc
