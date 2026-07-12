#include "cc/rules/RulePackLoader.hpp"
#include "cc/rules/RulePackValidator.hpp"
#include "cc/util/FileUtil.hpp"
#include "cc/util/StringUtil.hpp"

#include <optional>
#include <set>

namespace cc {
namespace {

[[nodiscard]] std::optional<std::string> objectString(const JsonValue::Object& object,
                                                      const std::string& key) {
    const auto iter = object.find(key);
    if (iter == object.end() || !iter->second.isString()) {
        return std::nullopt;
    }
    return iter->second.asString();
}

} // namespace

Result<std::vector<AuditRule>> RulePackLoader::loadDirectory(const std::filesystem::path& rulesDir,
                                                             CompetitionType track) const {
    std::vector<std::filesystem::path> files{rulesDir / "common_rules.json"};
    if (track == CompetitionType::BusinessInnovation) {
        files.push_back(rulesDir / "business_innovation_rules.json");
    } else if (track == CompetitionType::SoftwareProject ||
               track == CompetitionType::EngineeringProduct ||
               track == CompetitionType::AiApplication) {
        files.push_back(rulesDir / "software_project_rules.json");
    } else if (track == CompetitionType::ScientificResearch) {
        files.push_back(rulesDir / "research_project_rules.json");
    } else if (track == CompetitionType::SocialPractice ||
               track == CompetitionType::PublicWelfare) {
        files.push_back(rulesDir / "social_practice_rules.json");
    } else if (track == CompetitionType::Ecommerce) {
        files.push_back(rulesDir / "ecommerce_rules.json");
    } else if (track == CompetitionType::ComprehensiveInnovation) {
        files.push_back(rulesDir / "business_innovation_rules.json");
        files.push_back(rulesDir / "software_project_rules.json");
    }

    std::vector<AuditRule> rules;
    for (const auto& file : files) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(file, ec) || ec) {
            return Result<std::vector<AuditRule>>::failure("缺少规则包: " + util::pathString(file));
        }
        auto loaded = loadFile(file);
        if (!loaded.ok()) {
            return loaded;
        }
        rules.insert(rules.end(), loaded.value().begin(), loaded.value().end());
    }
    if (rules.empty()) {
        return Result<std::vector<AuditRule>>::failure("未找到可用规则包: " +
                                                       util::pathString(rulesDir));
    }
    const auto validated = RulePackValidator{}.validate(rules);
    if (!validated.ok()) {
        return Result<std::vector<AuditRule>>::failure(validated.error());
    }
    return Result<std::vector<AuditRule>>::success(rules);
}

Result<std::vector<AuditRule>> RulePackLoader::loadFile(const std::filesystem::path& file) const {
    constexpr std::uintmax_t kMaxRulePackBytes = 1024U * 1024U;
    std::error_code sizeError;
    const auto size = std::filesystem::file_size(file, sizeError);
    if (sizeError || size > kMaxRulePackBytes) {
        return Result<std::vector<AuditRule>>::failure("规则包不可读或超过 1 MiB: " +
                                                       util::pathString(file));
    }
    const auto content = util::readFileLimited(file, static_cast<std::size_t>(size) + 1U);
    if (content.empty()) {
        return Result<std::vector<AuditRule>>::failure("规则包为空或不可读: " +
                                                       util::pathString(file));
    }
    auto parsed = parseJson(content);
    if (!parsed.ok()) {
        return Result<std::vector<AuditRule>>::failure("规则包 JSON 解析失败: " + parsed.error());
    }

    const JsonValue* rulesValue = &parsed.value();
    if (parsed.value().isObject()) {
        const auto iter = parsed.value().asObject().find("rules");
        if (iter != parsed.value().asObject().end()) {
            rulesValue = &iter->second;
        }
    }
    if (!rulesValue->isArray()) {
        return Result<std::vector<AuditRule>>::failure("规则包必须是数组或包含 rules 数组");
    }

    std::vector<AuditRule> rules;
    const std::set<std::string, std::less<>> allowedFields{
        "rule_id",     "name",      "track",       "severity", "target",
        "description", "condition", "fail_reason", "fix_task"};
    for (const auto& item : rulesValue->asArray()) {
        if (!item.isObject()) {
            return Result<std::vector<AuditRule>>::failure("规则项必须是对象");
        }
        const auto& object = item.asObject();
        for (const auto& [key, _] : object) {
            if (!allowedFields.contains(key)) {
                return Result<std::vector<AuditRule>>::failure("规则包含未知字段: " + key);
            }
        }
        const auto severityText = objectString(object, "severity").value_or("");
        const auto severity = util::lowerAscii(severityText);
        if (severity != "blocker" && severity != "warning" && severity != "info") {
            return Result<std::vector<AuditRule>>::failure("规则 severity 无效: " + severityText);
        }
        AuditRule rule;
        rule.ruleId = objectString(object, "rule_id").value_or("");
        rule.name = objectString(object, "name").value_or("");
        rule.track = objectString(object, "track").value_or("common");
        rule.severity = severityFromString(severityText);
        rule.target = objectString(object, "target").value_or("");
        rule.description = objectString(object, "description").value_or("");
        const auto condition = object.find("condition");
        rule.condition = condition == object.end() ? JsonValue::Object{} : condition->second;
        rule.failReason = objectString(object, "fail_reason").value_or("");
        rule.fixTask = objectString(object, "fix_task").value_or("");
        rules.push_back(std::move(rule));
    }
    const auto validated = RulePackValidator{}.validate(rules);
    if (!validated.ok()) {
        return Result<std::vector<AuditRule>>::failure(validated.error());
    }
    return Result<std::vector<AuditRule>>::success(rules);
}

} // namespace cc
