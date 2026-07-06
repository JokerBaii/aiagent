/**
 * @file DiffVerifier.cpp
 * @brief 二次审计差分实现。
 */

#include "cc/audit/DiffVerifier.hpp"
#include "cc/util/FileUtil.hpp"

#include <sstream>

namespace cc {

Result<AuditDiff> DiffVerifier::diffFiles(const std::filesystem::path& oldAudit,
                                          const std::filesystem::path& newAudit) const {
    auto parseFile = [](const std::filesystem::path& path) -> Result<JsonValue> {
        const auto content = util::readFileLimited(path, 4U * 1024U * 1024U);
        if (content.empty()) {
            return Result<JsonValue>::failure("审计 JSON 不可读: " + util::pathString(path));
        }
        return parseJson(content);
    };
    auto oldJson = parseFile(oldAudit);
    auto newJson = parseFile(newAudit);
    if (!oldJson.ok()) {
        return Result<AuditDiff>::failure(oldJson.error());
    }
    if (!newJson.ok()) {
        return Result<AuditDiff>::failure(newJson.error());
    }

    auto metricInt = [](const JsonValue& root, const std::string& key) {
        return static_cast<int>(root.at("summary").at(key).asNumber(0.0));
    };
    auto metricDouble = [](const JsonValue& root, const std::string& key) {
        return root.at("summary").at(key).asNumber(0.0);
    };
    auto dimensionMetric = [](const JsonValue& root, const std::string& key,
                              const std::string& dimension) {
        const auto value = root.at("summary").at(key).asNumber(-1.0);
        if (value >= 0.0) {
            return static_cast<int>(value);
        }
        return static_cast<int>(
            root.at("trust_score").at("dimensions").at(dimension).asNumber(0.0));
    };

    AuditDiff diff;
    diff.oldScore = metricInt(oldJson.value(), "total_score");
    diff.newScore = metricInt(newJson.value(), "total_score");
    diff.oldTrustDebt = metricInt(oldJson.value(), "trust_debt");
    diff.newTrustDebt = metricInt(newJson.value(), "trust_debt");
    diff.oldBlockers = metricInt(oldJson.value(), "blocker_count");
    diff.newBlockers = metricInt(newJson.value(), "blocker_count");
    diff.oldWarnings = metricInt(oldJson.value(), "warning_count");
    diff.newWarnings = metricInt(newJson.value(), "warning_count");
    diff.oldEvidenceCoverage = metricDouble(oldJson.value(), "evidence_coverage");
    diff.newEvidenceCoverage = metricDouble(newJson.value(), "evidence_coverage");
    diff.oldMaterialCompleteness =
        dimensionMetric(oldJson.value(), "material_completeness", "材料完整性");
    diff.newMaterialCompleteness =
        dimensionMetric(newJson.value(), "material_completeness", "材料完整性");
    diff.oldConsistencyScore =
        dimensionMetric(oldJson.value(), "consistency_score", "项目逻辑自洽性");
    diff.newConsistencyScore =
        dimensionMetric(newJson.value(), "consistency_score", "项目逻辑自洽性");
    diff.oldFixTaskCount = metricInt(oldJson.value(), "fix_task_count");
    diff.newFixTaskCount = metricInt(newJson.value(), "fix_task_count");

    std::ostringstream summary;
    summary << "可信评分 " << diff.oldScore << " -> " << diff.newScore << "，blocker "
            << diff.oldBlockers << " -> " << diff.newBlockers << "，warning " << diff.oldWarnings
            << " -> " << diff.newWarnings << "，证据覆盖率 " << diff.oldEvidenceCoverage << "% -> "
            << diff.newEvidenceCoverage << "%，补证任务 " << diff.oldFixTaskCount << " -> "
            << diff.newFixTaskCount << "。";
    diff.summary = summary.str();
    return Result<AuditDiff>::success(diff);
}

} // namespace cc
