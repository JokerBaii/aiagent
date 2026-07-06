/**
 * @file AnalyzerRegistry.cpp
 * @brief 专用分析器注册器实现。
 */

#include "cc/agent/AnalyzerRegistry.hpp"
#include "cc/agent/BusinessAnalyzer.hpp"
#include "cc/agent/ConsistencyAnalyzer.hpp"
#include "cc/agent/EvidenceAnalyzer.hpp"
#include "cc/agent/ReportAnalyzer.hpp"
#include "cc/agent/ResearchAnalyzer.hpp"
#include "cc/agent/SecurityAnalyzer.hpp"
#include "cc/agent/SocialPracticeAnalyzer.hpp"
#include "cc/agent/SoftwareAnalyzer.hpp"

namespace cc {

std::vector<std::string> AnalyzerRegistry::builtinAnalyzerNames() const {
    return {"BusinessAnalyzer", "SoftwareAnalyzer",    "ResearchAnalyzer", "SocialPracticeAnalyzer",
            "EvidenceAnalyzer", "ConsistencyAnalyzer", "SecurityAnalyzer", "ReportAnalyzer"};
}

std::vector<std::unique_ptr<IAnalyzer>> AnalyzerRegistry::createAnalyzers() const {
    std::vector<std::unique_ptr<IAnalyzer>> analyzers;
    analyzers.push_back(std::make_unique<BusinessAnalyzer>());
    analyzers.push_back(std::make_unique<SoftwareAnalyzer>());
    analyzers.push_back(std::make_unique<ResearchAnalyzer>());
    analyzers.push_back(std::make_unique<SocialPracticeAnalyzer>());
    analyzers.push_back(std::make_unique<EvidenceAnalyzer>());
    analyzers.push_back(std::make_unique<ConsistencyAnalyzer>());
    analyzers.push_back(std::make_unique<SecurityAnalyzer>());
    analyzers.push_back(std::make_unique<ReportAnalyzer>());
    return analyzers;
}

} // namespace cc
