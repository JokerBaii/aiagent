/**
 * @file ToolRegistry.cpp
 * @brief 内置工具注册信息实现。
 */

#include "cc/agent/ToolRegistry.hpp"
#include "cc/core/Enums.hpp"

#include <cmath>
#include <cstddef>
#include <sstream>
#include <utility>

namespace cc {
namespace {

constexpr int kMaxListedFiles = 200;
constexpr int kMaxReadBytes = 64 * 1024;
constexpr int kMaxArchiveEntries = 200;
constexpr int kMaxSearchFiles = 200;
constexpr int kMaxSearchMatches = 100;
constexpr int kMaxQueryBytes = 512;
constexpr int kMaxPathBytes = 4096;
constexpr int kMaxInstructionBytes = 16 * 1024;
constexpr int kMaxWorkspaceContentBytes = 512 * 1024;
constexpr int kMaxEditAnchorBytes = 64 * 1024;
constexpr int kMaxEditOccurrences = 100;

[[nodiscard]] JsonValue emptyObjectSchema() {
    return JsonValue::Object{{"type", "object"},
                             {"properties", JsonValue::Object{}},
                             {"required", JsonValue::Array{}},
                             {"additionalProperties", false}};
}

[[nodiscard]] JsonValue property(const std::string& type, const std::string& description) {
    return JsonValue::Object{{"type", type}, {"description", description}};
}

[[nodiscard]] JsonValue integerProperty(const std::string& description, int minimum, int maximum) {
    return JsonValue::Object{{"type", "integer"},
                             {"description", description},
                             {"minimum", minimum},
                             {"maximum", maximum}};
}

[[nodiscard]] JsonValue stringProperty(const std::string& description, int minimumLength,
                                       int maximumLength) {
    return JsonValue::Object{{"type", "string"},
                             {"description", description},
                             {"minLength", minimumLength},
                             {"maxLength", maximumLength}};
}

[[nodiscard]] JsonValue objectSchema(JsonValue::Object properties, JsonValue::Array required = {}) {
    return JsonValue::Object{{"type", "object"},
                             {"properties", JsonValue{std::move(properties)}},
                             {"required", JsonValue{std::move(required)}},
                             {"additionalProperties", false}};
}

[[nodiscard]] AgentToolSpec spec(std::string name, std::string description,
                                 ToolPermission permission, JsonValue inputSchema,
                                 JsonValue outputSchema = emptyObjectSchema()) {
    return AgentToolSpec{.name = std::move(name),
                         .description = std::move(description),
                         .permission = permission,
                         .inputSchema = std::move(inputSchema),
                         .outputSchema = std::move(outputSchema)};
}

[[nodiscard]] bool containsKey(const JsonValue::Object& object, const std::string& key) {
    return object.find(key) != object.end();
}

[[nodiscard]] Result<void> validateProperty(const std::string& key, const JsonValue& value,
                                            const JsonValue& schema) {
    const auto type = schema.at("type").asString();
    if (type == "string") {
        if (!value.isString()) {
            return Result<void>::failure("工具参数 " + key + " 必须是字符串");
        }
        const auto size = value.asString().size();
        const auto minimum = schema.at("minLength").asNumber(0.0);
        const auto maximum = schema.at("maxLength").asNumber(-1.0);
        if (static_cast<double>(size) < minimum) {
            return Result<void>::failure("工具参数 " + key + " 不能为空");
        }
        if (maximum >= 0.0 && static_cast<double>(size) > maximum) {
            return Result<void>::failure("工具参数 " + key + " 超出允许长度");
        }
        return Result<void>::success();
    }
    if (type == "boolean") {
        return value.isBool() ? Result<void>::success()
                              : Result<void>::failure("工具参数 " + key + " 必须是布尔值");
    }
    if (type == "number" || type == "integer") {
        if (!value.isNumber()) {
            return Result<void>::failure("工具参数 " + key + " 必须是数值");
        }
        const auto number = value.asNumber();
        if (!std::isfinite(number)) {
            return Result<void>::failure("工具参数 " + key + " 必须是有限数值");
        }
        if (type == "integer" && std::floor(number) != number) {
            return Result<void>::failure("工具参数 " + key + " 必须是整数");
        }
        const auto minimum = schema.at("minimum").asNumber(-1.0e300);
        const auto maximum = schema.at("maximum").asNumber(1.0e300);
        if (number < minimum || number > maximum) {
            std::ostringstream message;
            message << "工具参数 " << key << " 必须在 " << minimum << " 到 " << maximum << " 之间";
            return Result<void>::failure(message.str());
        }
        return Result<void>::success();
    }
    if (type == "object") {
        return value.isObject() ? Result<void>::success()
                                : Result<void>::failure("工具参数 " + key + " 必须是对象");
    }
    return Result<void>::failure("工具参数 " + key + " 使用了运行时不支持的 schema 类型");
}

[[nodiscard]] Result<void> validateInput(const JsonValue& schema, const JsonValue& input) {
    if (!input.isObject()) {
        return Result<void>::failure("工具 input 必须是 JSON object");
    }
    const auto& inputObject = input.asObject();
    const auto& properties = schema.at("properties").asObject();
    for (const auto& required : schema.at("required").asArray()) {
        if (!required.isString()) {
            return Result<void>::failure("工具 schema 的 required 配置无效");
        }
        if (!containsKey(inputObject, required.asString())) {
            return Result<void>::failure("工具调用缺少必填参数: " + required.asString());
        }
    }
    for (const auto& [key, value] : inputObject) {
        const auto property = properties.find(key);
        if (property == properties.end()) {
            return Result<void>::failure("工具调用包含未声明参数: " + key);
        }
        auto valid = validateProperty(key, value, property->second);
        if (!valid.ok()) {
            return valid;
        }
    }
    return Result<void>::success();
}

} // namespace

std::vector<AgentToolSpec> ToolRegistry::interactiveToolSpecs() const {
    return {
        spec("run_project_audit",
             "运行完整的确定性项目审计：建立隔离副本，依次整理材料、抽取文本和声明、"
             "匹配证据、检查一致性、执行规则、计算评分并生成补证任务；首次项目评审必须调用",
             ToolPermission::ReadProjectFiles, emptyObjectSchema(),
             objectSchema({{"summary", property("string", "确定性审计摘要")},
                           {"score", property("number", "规则引擎计算的可信评分")},
                           {"finding_count", property("number", "规则风险数量")},
                           {"fix_task_count", property("number", "补证任务数量")}})),
        spec("summarize_audit_session", "读取当前审计会话摘要、风险、证据缺口和补证任务",
             ToolPermission::ReadProjectFiles, emptyObjectSchema(),
             objectSchema({{"summary", property("string", "面向用户的审计上下文摘要")}})),
        spec("list_project_files", "枚举允许读取的项目副本文件，用于让 Brain 自动翻阅材料",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"max_files", integerProperty("最多返回文件数量，默认 80", 1, kMaxListedFiles)}}),
             objectSchema(
                 {{"files", JsonValue::Object{{"type", "array"},
                                              {"description", "带格式元数据的文件列表"}}}})),
        spec("inspect_project_file", "查看单个项目文件的格式、MIME、语言、大小和可用读取策略",
             ToolPermission::ReadProjectFiles,
             objectSchema({{"path", stringProperty("项目内相对路径；项目本身是单文件时可留空", 0,
                                                   kMaxPathBytes)}}),
             objectSchema({{"asset", property("object", "文件格式和读取策略元数据")},
                           {"can_read_text", property("boolean", "是否可用 read_text_file 读取")},
                           {"can_read_extracted_document",
                            property("boolean", "是否可读取办公/PDF 项目文件的抽取内容")},
                           {"can_inspect_archive",
                            property("boolean", "是否可用 inspect_archive 查看包内容")},
                           {"suggested_tool", property("string", "建议下一步工具")}})),
        spec("read_text_file", "读取项目副本中的文本、代码或结构化配置文件片段",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"path", stringProperty("项目内相对路径", 1, kMaxPathBytes)},
                  {"max_bytes", integerProperty("最多读取字节数，默认 12000", 1, kMaxReadBytes)}},
                 JsonValue::Array{"path"}),
             objectSchema({{"path", property("string", "已读取的相对路径")},
                           {"asset", property("object", "文件格式和语言元数据")},
                           {"content", property("string", "文件内容片段")}})),
        spec("read_extracted_document",
             "读取真实项目中的 PDF、DOCX、PPTX、XLSX 办公文件内容；"
             "保留抽取状态，不把扫描件或截断内容冒充完整正文",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"path", stringProperty("项目内文档相对路径", 1, kMaxPathBytes)},
                  {"max_bytes", integerProperty("最多返回字节数，默认 24000", 1, kMaxReadBytes)}},
                 JsonValue::Array{"path"}),
             objectSchema({{"path", property("string", "文档相对路径")},
                           {"status", property("string", "抽取完整性状态")},
                           {"extracted_from_project_file",
                            property("boolean", "内容是否来自用户导入的真实项目文件")},
                           {"content", property("string", "受限长度的抽取文本")}})),
        spec("inspect_archive", "列出项目压缩包或代码包的内部条目，不解包、不执行其中内容",
             ToolPermission::ReadProjectFiles,
             objectSchema({{"path", stringProperty("项目内压缩包路径；项目本身是压缩包时可留空", 0,
                                                   kMaxPathBytes)},
                           {"max_entries",
                            integerProperty("最多返回条目数，默认 120", 1, kMaxArchiveEntries)}}),
             objectSchema({{"entries", JsonValue::Object{{"type", "array"},
                                                         {"description", "压缩包条目摘要"}}},
                           {"safe_to_extract",
                            property("boolean", "是否未发现路径穿越、符号链接或嵌套压缩包")},
                           {"supported", property("boolean", "是否为当前 reader 可枚举格式")}})),
        spec("search_project_text", "在项目副本文本文件中搜索关键词，返回命中的路径和行号",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"query", stringProperty("要搜索的文本关键词", 1, kMaxQueryBytes)},
                  {"max_files", integerProperty("最多扫描文件数量，默认 80", 1, kMaxSearchFiles)},
                  {"max_matches",
                   integerProperty("最多返回命中数量，默认 40", 1, kMaxSearchMatches)},
                  {"case_sensitive", property("boolean", "是否区分大小写，默认 false")}},
                 JsonValue::Array{"query"}),
             objectSchema({{"matches",
                            JsonValue::Object{{"type", "array"}, {"description", "命中行列表"}}}})),
        spec("prepare_repaired_workspace",
             "把隔离项目复制成 repaired project，后续修改只发生在该副本",
             ToolPermission::WriteWorkspace, emptyObjectSchema(),
             objectSchema({{"repaired_root", property("string", "符号化 repaired project 根")}})),
        spec(
            "apply_repaired_text_edit",
            "在 repaired project 中执行带旧文本锚点和命中数校验的精确修改，并更新可应用 diff",
            ToolPermission::WriteWorkspace,
            objectSchema(
                {{"path", stringProperty("项目内相对路径", 1, kMaxPathBytes)},
                 {"expected_text", stringProperty("必须精确匹配的旧文本", 1, kMaxEditAnchorBytes)},
                 {"replacement_text", stringProperty("替换后的文本", 0, kMaxWorkspaceContentBytes)},
                 {"expected_occurrences",
                  integerProperty("旧文本期望命中次数，默认 1", 1, kMaxEditOccurrences)}},
                JsonValue::Array{"path", "expected_text", "replacement_text"}),
            objectSchema({{"path", property("string", "已修改文件")},
                          {"patch", property("string", "统一 diff 产物")},
                          {"diff", property("string", "本次文件差分")},
                          {"preview", property("string", "修改后预览")}})),
        spec("create_repaired_text_file",
             "在 repaired project 新建文本、源码或配置文件，并更新可应用 diff；拒绝覆盖已有文件",
             ToolPermission::WriteWorkspace,
             objectSchema({{"path", stringProperty("项目内相对路径", 1, kMaxPathBytes)},
                           {"content", stringProperty("新文件内容", 1, kMaxWorkspaceContentBytes)}},
                          JsonValue::Array{"path", "content"}),
             objectSchema({{"path", property("string", "已创建文件")},
                           {"patch", property("string", "统一 diff 产物")},
                           {"diff", property("string", "本次文件差分")},
                           {"preview", property("string", "文件预览")}})),
        spec("read_repaired_text_file", "读回 repaired project 文件以验证修改结果",
             ToolPermission::WriteWorkspace,
             objectSchema(
                 {{"path", stringProperty("项目内相对路径", 1, kMaxPathBytes)},
                  {"max_bytes", integerProperty("最多返回字节数，默认 24000", 1, kMaxReadBytes)}},
                 JsonValue::Array{"path"}),
             objectSchema({{"path", property("string", "工作区相对路径")},
                           {"content", property("string", "修改后的文本")}})),
        spec("list_workspace_changes", "列出 repaired project 的真实变更和统一 diff",
             ToolPermission::WriteWorkspace, emptyObjectSchema(),
             objectSchema({{"changes", property("array", "变更文件列表")},
                           {"patch", property("string", "组合补丁路径")}})),
        spec("re_audit_repaired_project",
             "对 repaired project 重新执行确定性审计并生成修复前后差分",
             ToolPermission::WriteWorkspace, emptyObjectSchema(),
             objectSchema({{"summary", property("string", "二次审计差分摘要")},
                           {"old_score", property("number", "修复前评分")},
                           {"new_score", property("number", "修复后评分")}})),
        spec("draft_markdown_revision", "基于读取到的 Markdown 生成工作区修订稿，不覆盖原项目文件",
             ToolPermission::WriteWorkspace,
             objectSchema({{"path", stringProperty("项目内 Markdown 相对路径", 1, kMaxPathBytes)},
                           {"instruction", stringProperty("修订目标", 0, kMaxInstructionBytes)},
                           {"replacement_markdown", stringProperty("可选，Brain 生成的完整替换稿",
                                                                   0, kMaxWorkspaceContentBytes)}},
                          JsonValue::Array{"path"}),
             objectSchema({{"workspace_path", property("string", "修订稿写入路径")},
                           {"preview", property("string", "修订稿预览")}})),
        spec("write_workspace_file", "把 Brain 生成的文本、代码、配置或报告写入会话工作区",
             ToolPermission::WriteWorkspace,
             objectSchema(
                 {{"path", stringProperty("工作区内相对路径", 1, kMaxPathBytes)},
                  {"content", stringProperty("要写入的文本内容", 0, kMaxWorkspaceContentBytes)}},
                 JsonValue::Array{"path", "content"}),
             objectSchema({{"workspace_path", property("string", "写入后的工作区路径")},
                           {"format", property("string", "根据扩展名推断的输出格式")},
                           {"preview", property("string", "写入内容预览")}}))};
}

Result<void> ToolRegistry::validateInteractiveInput(const std::string& toolName,
                                                    const JsonValue& input) const {
    const auto specs = interactiveToolSpecs();
    for (const auto& tool : specs) {
        if (tool.name == toolName) {
            return validateInput(tool.inputSchema, input);
        }
    }
    return Result<void>::failure("未注册工具: " + toolName);
}

} // namespace cc
