/**
 * @file ProjectModels.hpp
 * @brief 项目上下文、资产清单、文本和 CPIR 数据模型。
 */

#pragma once

#include "cc/core/Enums.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cc {

/**
 * @brief 项目导入后的安全上下文。
 *
 * originalRoot 保留用户输入来源，inputRoot 指向隔离工作区中的审计副本；后续扫描和修复
 * 都应基于 inputRoot，避免误读或覆盖原项目。
 */
struct ProjectContext {
    std::filesystem::path originalRoot;
    std::filesystem::path inputRoot;
    std::filesystem::path workspaceRoot;
    std::string sessionId;
    std::string projectName;
    std::string unpackStatus;
    bool archiveInput{false};
    std::vector<std::filesystem::path> inputFiles;
    std::vector<std::string> warnings;
};

/**
 * @brief 单个项目文件的资产语义信息。
 *
 * 资产模型同时记录格式、角色、风险和是否可审计，后续规则和报告必须基于这些确定性字段，
 * 不能再凭文件名临时猜测。
 */
struct ProjectAsset {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    std::string fileName;
    std::string extension;
    std::uintmax_t sizeBytes{0};
    std::string format;
    std::string mime;
    std::string language;
    AssetRole role{AssetRole::Unknown};
    int importance{1};
    bool auditable{false};
    bool generated{false};
    bool vendored{false};
    bool sensitive{false};
    std::vector<std::string> riskFlags;
};

/**
 * @brief 项目材料包的资产清单。
 *
 * roleCounts 和 warnings 为报告、规则引擎和 Workbench 提供可复核摘要，避免 UI 或报告层
 * 重复计算资产语义。
 */
struct ProjectInventory {
    std::filesystem::path root;
    std::vector<ProjectAsset> assets;
    std::map<AssetRole, std::size_t> roleCounts;
    std::vector<std::string> warnings;
};

/**
 * @brief 从可审计材料中抽取出的文本。
 *
 * status 会记录抽取状态；PDF 扫描件或解析失败材料必须显式标记为 NEED_REVIEW，不能由系统
 * 幻觉解释。
 */
struct TextDocument {
    std::filesystem::path sourceFile;
    std::string title;
    std::string text;
    std::string status;
};

/**
 * @brief 竞赛类型识别结果。
 *
 * 除最终类型外保留置信度和判断理由，便于用户复核自动识别是否需要通过 --track 覆盖。
 */
struct CompetitionTypeResult {
    CompetitionType type{CompetitionType::Unknown};
    double confidence{0.0};
    std::string reason;
};

/**
 * @brief 竞赛项目中间表示。
 *
 * CPIR 将不同赛道材料统一成结构化字段；缺失字段进入 missingFields/riskItems，不允许用
 * 生成式文本补空。
 */
struct CPIR {
    std::string projectName;
    CompetitionType competitionType{CompetitionType::Unknown};
    double competitionConfidence{0.0};
    std::string competitionReason;
    std::string track;
    std::string targetUser;
    std::string painPoint;
    std::string solution;
    std::string productOrService;
    std::string technicalRoute;
    std::string businessModel;
    std::string marketAnalysis;
    std::string competitorAnalysis;
    std::string financialProjection;
    std::string teamStructure;
    std::string currentResults;
    std::string socialValue;
    std::vector<std::string> missingFields;
    std::vector<std::string> riskItems;
};

} // namespace cc
