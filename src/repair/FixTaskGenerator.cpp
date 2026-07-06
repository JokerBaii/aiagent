/**
 * @file FixTaskGenerator.cpp
 * @brief 补证任务生成实现。
 */

#include "cc/repair/FixTaskGenerator.hpp"

#include <iomanip>
#include <sstream>

namespace cc {

std::vector<FixTask> FixTaskGenerator::generate(const std::vector<AuditFinding>& findings,
                                                const std::vector<EvidenceMatch>& matches) const {
    std::vector<FixTask> tasks;
    std::size_t counter = 1;
    auto nextId = [&]() {
        std::ostringstream id;
        id << "FIX-" << std::setw(3) << std::setfill('0') << counter++;
        return id.str();
    };

    for (const auto& finding : findings) {
        FixTask task;
        task.taskId = nextId();
        task.title = finding.fixSuggestion.empty() ? finding.title : finding.fixSuggestion;
        task.priority = finding.severity == Severity::Blocker ? "P0" : "P1";
        task.reason = finding.reason;
        task.requiredMaterial = finding.missingEvidence;
        task.affectedRules.push_back(finding.ruleId);
        task.relatedFiles = finding.evidence;
        tasks.push_back(std::move(task));
    }
    for (const auto& match : matches) {
        if (match.status == EvidenceStatus::Supported) {
            continue;
        }
        FixTask task;
        task.taskId = nextId();
        task.title = "为声明 " + match.claimId + " 补充独立证据";
        task.priority = match.status == EvidenceStatus::Unsupported ? "P0" : "P1";
        task.reason = match.reason;
        task.requiredMaterial = match.missingEvidence;
        task.affectedRules.push_back("EVIDENCE_" + match.claimId);
        task.relatedFiles = match.evidenceFiles;
        tasks.push_back(std::move(task));
    }
    return tasks;
}

} // namespace cc
