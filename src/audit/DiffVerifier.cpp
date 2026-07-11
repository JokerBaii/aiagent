#include "cc/audit/DiffVerifier.hpp"
#include "cc/util/FileUtil.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace cc {
namespace {

constexpr std::uintmax_t kMaxAuditPackageBytes = 64U * 1024U * 1024U;

struct AuditMetrics {
    int score{0};
    int trustDebt{0};
    int blockers{0};
    int warnings{0};
    double evidenceCoverage{0.0};
    int materialCompleteness{0};
    int consistencyScore{0};
    int fixTaskCount{0};
};

[[nodiscard]] Result<double> requiredNumber(const JsonValue& object, const std::string& key,
                                            double minimum, double maximum,
                                            bool integerOnly = false) {
    if (!object.isObject()) {
        return Result<double>::failure("审计报告字段不是对象: " + key);
    }
    const auto& value = object.at(key);
    const auto number = value.asNumber(std::numeric_limits<double>::quiet_NaN());
    if (!value.isNumber() || !std::isfinite(number) || number < minimum || number > maximum ||
        (integerOnly && std::floor(number) != number)) {
        return Result<double>::failure("审计报告字段缺失或越界: " + key);
    }
    return Result<double>::success(number);
}

[[nodiscard]] Result<int> dimensionMetric(const JsonValue& root, const std::string& summaryKey,
                                          const std::string& dimension) {
    const auto& summaryValue = root.at("summary").at(summaryKey);
    if (summaryValue.isNumber()) {
        const auto checked = requiredNumber(root.at("summary"), summaryKey, 0.0, 100.0, true);
        if (!checked.ok()) {
            return Result<int>::failure(checked.error());
        }
        return Result<int>::success(static_cast<int>(checked.value()));
    }
    const auto checked = requiredNumber(root.at("trust_score").at("dimensions"), dimension, 0.0,
                                        100.0, true);
    if (!checked.ok()) {
        return Result<int>::failure("审计报告缺少维度 " + dimension);
    }
    return Result<int>::success(static_cast<int>(checked.value()));
}

[[nodiscard]] Result<AuditMetrics> metricsFromJson(const JsonValue& root) {
    if (!root.isObject() || !root.at("summary").isObject()) {
        return Result<AuditMetrics>::failure("审计 JSON 缺少 summary 对象");
    }
    const auto& summary = root.at("summary");
    const auto score = requiredNumber(summary, "total_score", 0.0, 100.0, true);
    const auto debt = requiredNumber(summary, "trust_debt", 0.0, 100.0, true);
    const auto blockers = requiredNumber(summary, "blocker_count", 0.0, 1000000.0, true);
    const auto warnings = requiredNumber(summary, "warning_count", 0.0, 1000000.0, true);
    const auto coverage = requiredNumber(summary, "evidence_coverage", 0.0, 100.0);
    const auto taskCount = requiredNumber(summary, "fix_task_count", 0.0, 1000000.0, true);
    const auto material = dimensionMetric(root, "material_completeness", "材料完整性");
    const auto consistency = dimensionMetric(root, "consistency_score", "项目逻辑自洽性");
    if (!score.ok() || !debt.ok() || !blockers.ok() || !warnings.ok() || !coverage.ok() ||
        !taskCount.ok() || !material.ok() || !consistency.ok()) {
        const Result<double>* failures[]{&score, &debt, &blockers, &warnings, &coverage,
                                         &taskCount};
        for (const auto* failure : failures) {
            if (!failure->ok()) {
                return Result<AuditMetrics>::failure(failure->error());
            }
        }
        return Result<AuditMetrics>::failure(!material.ok() ? material.error()
                                                            : consistency.error());
    }

    AuditMetrics metrics;
    metrics.score = static_cast<int>(score.value());
    metrics.trustDebt = static_cast<int>(debt.value());
    metrics.blockers = static_cast<int>(blockers.value());
    metrics.warnings = static_cast<int>(warnings.value());
    metrics.evidenceCoverage = coverage.value();
    metrics.materialCompleteness = material.value();
    metrics.consistencyScore = consistency.value();
    metrics.fixTaskCount = static_cast<int>(taskCount.value());
    return Result<AuditMetrics>::success(metrics);
}

[[nodiscard]] int findingCount(const AuditResult& result, Severity severity) {
    return static_cast<int>(std::count_if(
        result.findings.begin(), result.findings.end(),
        [severity](const AuditFinding& finding) { return finding.severity == severity; }));
}

[[nodiscard]] int taskCount(const AuditResult& result, const std::string& priority) {
    return static_cast<int>(std::count_if(
        result.fixTasks.begin(), result.fixTasks.end(),
        [&](const FixTask& task) { return task.priority == priority; }));
}

[[nodiscard]] AuditMetrics metricsFromResult(const AuditResult& result) {
    AuditMetrics metrics;
    metrics.score = result.trustScore.totalScore;
    metrics.trustDebt = result.trustScore.trustDebt;
    metrics.blockers = std::max(findingCount(result, Severity::Blocker), taskCount(result, "P0"));
    metrics.warnings = std::max(findingCount(result, Severity::Warning), taskCount(result, "P1"));
    if (!result.evidenceMatches.empty()) {
        double covered = 0.0;
        for (const auto& match : result.evidenceMatches) {
            covered += match.status == EvidenceStatus::Supported
                           ? 1.0
                           : (match.status == EvidenceStatus::Partial ? 0.5 : 0.0);
        }
        metrics.evidenceCoverage =
            covered * 100.0 / static_cast<double>(result.evidenceMatches.size());
    }
    const auto material = result.trustScore.dimensions.find("材料完整性");
    const auto consistency = result.trustScore.dimensions.find("项目逻辑自洽性");
    metrics.materialCompleteness =
        material == result.trustScore.dimensions.end() ? 0 : material->second;
    metrics.consistencyScore =
        consistency == result.trustScore.dimensions.end() ? 0 : consistency->second;
    metrics.fixTaskCount = static_cast<int>(result.fixTasks.size());
    return metrics;
}

[[nodiscard]] AuditDiff makeDiff(const AuditMetrics& oldMetrics,
                                 const AuditMetrics& newMetrics) {
    AuditDiff diff;
    diff.oldScore = oldMetrics.score;
    diff.newScore = newMetrics.score;
    diff.oldTrustDebt = oldMetrics.trustDebt;
    diff.newTrustDebt = newMetrics.trustDebt;
    diff.oldBlockers = oldMetrics.blockers;
    diff.newBlockers = newMetrics.blockers;
    diff.oldWarnings = oldMetrics.warnings;
    diff.newWarnings = newMetrics.warnings;
    diff.oldEvidenceCoverage = oldMetrics.evidenceCoverage;
    diff.newEvidenceCoverage = newMetrics.evidenceCoverage;
    diff.oldMaterialCompleteness = oldMetrics.materialCompleteness;
    diff.newMaterialCompleteness = newMetrics.materialCompleteness;
    diff.oldConsistencyScore = oldMetrics.consistencyScore;
    diff.newConsistencyScore = newMetrics.consistencyScore;
    diff.oldFixTaskCount = oldMetrics.fixTaskCount;
    diff.newFixTaskCount = newMetrics.fixTaskCount;

    std::ostringstream summary;
    summary << "可信评分 " << diff.oldScore << " -> " << diff.newScore << "，blocker "
            << diff.oldBlockers << " -> " << diff.newBlockers << "，warning "
            << diff.oldWarnings << " -> " << diff.newWarnings << "，证据覆盖率 "
            << diff.oldEvidenceCoverage << "% -> " << diff.newEvidenceCoverage << "%，补证任务 "
            << diff.oldFixTaskCount << " -> " << diff.newFixTaskCount << "。";
    diff.summary = summary.str();
    return diff;
}

} // namespace

Result<AuditDiff> DiffVerifier::diffFiles(const std::filesystem::path& oldAudit,
                                          const std::filesystem::path& newAudit) const {
    auto parseFile = [](const std::filesystem::path& path) -> Result<JsonValue> {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size == 0U) {
            return Result<JsonValue>::failure("审计 JSON 不可读: " + util::pathString(path));
        }
        if (size > kMaxAuditPackageBytes) {
            return Result<JsonValue>::failure("审计 JSON 超过 64 MiB 安全上限: " +
                                              util::pathString(path));
        }
        const auto content = util::readFileLimited(path, static_cast<std::size_t>(size) + 1U);
        if (content.size() != size) {
            return Result<JsonValue>::failure("审计 JSON 未完整读取: " + util::pathString(path));
        }
        auto parsed = parseJson(content);
        if (!parsed.ok()) {
            return Result<JsonValue>::failure("审计 JSON 解析失败: " + parsed.error());
        }
        return parsed;
    };

    auto oldJson = parseFile(oldAudit);
    if (!oldJson.ok()) {
        return Result<AuditDiff>::failure(oldJson.error());
    }
    auto newJson = parseFile(newAudit);
    if (!newJson.ok()) {
        return Result<AuditDiff>::failure(newJson.error());
    }
    return diffJson(oldJson.value(), newJson.value());
}

Result<AuditDiff> DiffVerifier::diffResults(const AuditResult& oldAudit,
                                            const AuditResult& newAudit) const {
    return Result<AuditDiff>::success(makeDiff(metricsFromResult(oldAudit),
                                               metricsFromResult(newAudit)));
}

Result<AuditDiff> DiffVerifier::diffJson(const JsonValue& oldAudit,
                                         const JsonValue& newAudit) const {
    const auto oldMetrics = metricsFromJson(oldAudit);
    if (!oldMetrics.ok()) {
        return Result<AuditDiff>::failure("旧审计报告无效: " + oldMetrics.error());
    }
    const auto newMetrics = metricsFromJson(newAudit);
    if (!newMetrics.ok()) {
        return Result<AuditDiff>::failure("新审计报告无效: " + newMetrics.error());
    }
    return Result<AuditDiff>::success(makeDiff(oldMetrics.value(), newMetrics.value()));
}

} // namespace cc
