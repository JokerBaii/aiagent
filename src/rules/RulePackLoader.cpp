/**
 * @file RulePackLoader.cpp
 * @brief JSON 规则包加载实现。
 */

#include "cc/rules/RulePackLoader.hpp"
#include "cc/util/FileUtil.hpp"

#include <optional>

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
               track == CompetitionType::AiApplication) {
        files.push_back(rulesDir / "software_project_rules.json");
    } else if (track == CompetitionType::ScientificResearch) {
        files.push_back(rulesDir / "research_project_rules.json");
    } else if (track == CompetitionType::SocialPractice) {
        files.push_back(rulesDir / "social_practice_rules.json");
    } else if (track == CompetitionType::Ecommerce) {
        files.push_back(rulesDir / "ecommerce_rules.json");
    } else {
        files.push_back(rulesDir / "business_innovation_rules.json");
        files.push_back(rulesDir / "software_project_rules.json");
    }

    std::vector<AuditRule> rules;
    for (const auto& file : files) {
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            continue;
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
    return Result<std::vector<AuditRule>>::success(rules);
}

Result<std::vector<AuditRule>> RulePackLoader::loadFile(const std::filesystem::path& file) const {
    const auto content = util::readFileLimited(file, 1024U * 1024U);
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
    for (const auto& item : rulesValue->asArray()) {
        if (!item.isObject()) {
            return Result<std::vector<AuditRule>>::failure("规则项必须是对象");
        }
        const auto& object = item.asObject();
        AuditRule rule;
        rule.ruleId = objectString(object, "rule_id").value_or("");
        rule.name = objectString(object, "name").value_or("");
        rule.track = objectString(object, "track").value_or("common");
        rule.severity = severityFromString(objectString(object, "severity").value_or("warning"));
        rule.target = objectString(object, "target").value_or("");
        rule.description = objectString(object, "description").value_or("");
        const auto condition = object.find("condition");
        rule.condition = condition == object.end() ? JsonValue::Object{} : condition->second;
        rule.failReason = objectString(object, "fail_reason").value_or("");
        rule.fixTask = objectString(object, "fix_task").value_or("");
        rules.push_back(std::move(rule));
    }
    return Result<std::vector<AuditRule>>::success(rules);
}

} // namespace cc
