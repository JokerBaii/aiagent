#include "cc/repair/FixTaskGenerator.hpp"
#include "cc/util/StringUtil.hpp"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

namespace cc {
namespace {

template <typename T> void appendUnique(std::vector<T>& target, const std::vector<T>& values) {
    for (const auto& value : values) {
        if (std::find(target.begin(), target.end(), value) == target.end()) {
            target.push_back(value);
        }
    }
}

[[nodiscard]] int priorityRank(const std::string& priority) {
    if (priority == "P0") {
        return 0;
    }
    return priority == "P1" ? 1 : 2;
}

[[nodiscard]] std::string taskKey(const FixTask& task) {
    if (task.requiredMaterial.empty()) {
        return util::lowerAscii(util::trim(task.title));
    }
    auto material = task.requiredMaterial;
    for (auto& item : material) {
        item = util::lowerAscii(util::trim(item));
    }
    std::sort(material.begin(), material.end());
    material.erase(std::unique(material.begin(), material.end()), material.end());
    return util::join(material, "|");
}

void mergeTask(FixTask& target, FixTask source) {
    if (priorityRank(source.priority) < priorityRank(target.priority)) {
        target.priority = std::move(source.priority);
    }
    appendUnique(target.requiredMaterial, source.requiredMaterial);
    appendUnique(target.affectedRules, source.affectedRules);
    appendUnique(target.relatedFiles, source.relatedFiles);
}

[[nodiscard]] std::filesystem::path claimSource(
    const std::string& claimId, const std::vector<ProjectClaim>& claims) {
    const auto claim = std::find_if(claims.begin(), claims.end(), [&](const ProjectClaim& item) {
        return item.claimId == claimId;
    });
    return claim == claims.end() ? std::filesystem::path{} : claim->sourceFile;
}

} // namespace

std::vector<FixTask> FixTaskGenerator::generate(const std::vector<AuditFinding>& findings,
                                                const std::vector<EvidenceMatch>& matches,
                                                const std::vector<ProjectClaim>& claims) const {
    std::vector<FixTask> tasks;
    std::map<std::string, std::size_t> grouped;
    auto add = [&](FixTask task) {
        const auto key = taskKey(task);
        const auto existing = grouped.find(key);
        if (existing != grouped.end()) {
            mergeTask(tasks[existing->second], std::move(task));
            return;
        }
        grouped.emplace(key, tasks.size());
        tasks.push_back(std::move(task));
    };

    for (const auto& finding : findings) {
        FixTask task;
        task.title = finding.fixSuggestion.empty() ? finding.title : finding.fixSuggestion;
        task.priority = finding.severity == Severity::Blocker
                            ? "P0"
                            : (finding.severity == Severity::Warning ? "P1" : "P2");
        task.reason = finding.reason;
        task.requiredMaterial = finding.missingEvidence;
        task.affectedRules.push_back(finding.ruleId);
        task.relatedFiles = finding.evidence;
        add(std::move(task));
    }
    for (const auto& match : matches) {
        if (match.status == EvidenceStatus::Supported) {
            continue;
        }
        FixTask task;
        task.title = "为声明 " + match.claimId + " 补充独立证据";
        task.priority = match.status == EvidenceStatus::Unsupported ||
                                match.status == EvidenceStatus::Conflicted
                            ? "P0"
                            : "P1";
        task.reason = match.reason;
        task.requiredMaterial = match.missingEvidence;
        if (task.requiredMaterial.empty()) {
            task.requiredMaterial.emplace_back(match.status == EvidenceStatus::NeedReview
                                                   ? "人工复核声明原文与证据"
                                                   : "可独立复核的来源材料");
        }
        task.affectedRules.push_back("EVIDENCE_" + match.claimId);
        task.relatedFiles = match.evidenceFiles;
        const auto source = claimSource(match.claimId, claims);
        if (!source.empty()) {
            task.relatedFiles.push_back(source);
        }
        add(std::move(task));
    }

    std::stable_sort(tasks.begin(), tasks.end(), [](const FixTask& left, const FixTask& right) {
        return priorityRank(left.priority) < priorityRank(right.priority);
    });
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        std::ostringstream id;
        id << "FIX-" << std::setw(3) << std::setfill('0') << index + 1U;
        tasks[index].taskId = id.str();
        const auto evidenceRules = static_cast<std::size_t>(std::count_if(
            tasks[index].affectedRules.begin(), tasks[index].affectedRules.end(),
            [](const std::string& rule) { return rule.starts_with("EVIDENCE_"); }));
        if (evidenceRules > 1U) {
            tasks[index].title = "为 " + std::to_string(evidenceRules) + " 条声明补充同类独立证据";
        }
    }
    return tasks;
}

} // namespace cc
