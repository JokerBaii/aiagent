/**
 * @file EvidenceAnalyzer.cpp
 * @brief 证据覆盖确定性分析器实现。
 */

#include "cc/agent/EvidenceAnalyzer.hpp"

namespace cc {

std::string EvidenceAnalyzer::name() const {
    return "EvidenceAnalyzer";
}

bool EvidenceAnalyzer::supports(CompetitionType) const {
    return true;
}

Result<std::vector<AuditFinding>>
EvidenceAnalyzer::analyze(const CPIR&, const ProjectInventory&,
                          const std::vector<EvidenceMatch>& evidenceGraph) const {
    std::vector<std::string> missing;
    for (const auto& match : evidenceGraph) {
        if (match.status == EvidenceStatus::Unsupported) {
            missing.insert(missing.end(), match.missingEvidence.begin(),
                           match.missingEvidence.end());
        }
    }
    std::vector<AuditFinding> findings;
    if (!missing.empty()) {
        AuditFinding finding;
        finding.ruleId = "ANALYZER_EVIDENCE_COVERAGE";
        finding.severity = Severity::Warning;
        finding.title = "声明证据覆盖不足";
        finding.reason = "存在未被独立证据支撑的声明，提交前需要补齐证据链。";
        finding.missingEvidence = std::move(missing);
        finding.fixSuggestion = "优先补充未支撑声明对应的独立材料。";
        findings.push_back(std::move(finding));
    }
    return Result<std::vector<AuditFinding>>::success(findings);
}

} // namespace cc
