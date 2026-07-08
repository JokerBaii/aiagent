/**
 * @file ToolRegistry.cpp
 * @brief 内置工具注册信息实现。
 */

#include "cc/agent/ToolRegistry.hpp"
#include "cc/core/Enums.hpp"

namespace cc {
namespace {

[[nodiscard]] JsonValue emptyObjectSchema() {
    return JsonValue::Object{{"type", "object"}, {"properties", JsonValue::Object{}}};
}

[[nodiscard]] JsonValue property(const std::string& type, const std::string& description) {
    return JsonValue::Object{{"type", type}, {"description", description}};
}

[[nodiscard]] JsonValue objectSchema(JsonValue::Object properties) {
    return JsonValue::Object{{"type", "object"}, {"properties", JsonValue{std::move(properties)}}};
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

} // namespace

std::vector<AgentToolSpec> ToolRegistry::interactiveToolSpecs() const {
    return {
        spec("summarize_audit_session", "读取当前审计会话摘要、风险、证据缺口和补证任务",
             ToolPermission::ReadProjectFiles, emptyObjectSchema(),
             objectSchema({{"summary", property("string", "面向用户的审计上下文摘要")}})),
        spec("list_project_files", "枚举允许读取的项目副本文件，用于让 Brain 自动翻阅材料",
             ToolPermission::ReadProjectFiles,
             objectSchema({{"max_files", property("number", "最多返回文件数量，默认 80")}}),
             objectSchema(
                 {{"files", JsonValue::Object{{"type", "array"},
                                              {"description", "带格式元数据的文件列表"}}}})),
        spec("inspect_project_file", "查看单个项目文件的格式、MIME、语言、大小和可用读取策略",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"path", property("string", "项目内相对路径；项目本身是单文件时可留空")}}),
             objectSchema({{"asset", property("object", "文件格式和读取策略元数据")},
                           {"can_read_text", property("boolean", "是否可用 read_text_file 读取")},
                           {"can_inspect_archive",
                            property("boolean", "是否可用 inspect_archive 查看包内容")},
                           {"suggested_tool", property("string", "建议下一步工具")}})),
        spec("read_text_file", "读取项目副本中的文本、代码或结构化配置文件片段",
             ToolPermission::ReadProjectFiles,
             objectSchema({{"path", property("string", "项目内相对路径")},
                           {"max_bytes", property("number", "最多读取字节数，默认 12000")}}),
             objectSchema({{"path", property("string", "已读取的相对路径")},
                           {"asset", property("object", "文件格式和语言元数据")},
                           {"content", property("string", "文件内容片段")}})),
        spec("inspect_archive", "列出项目压缩包或代码包的内部条目，不解包、不执行其中内容",
             ToolPermission::ReadProjectFiles,
             objectSchema(
                 {{"path", property("string", "项目内压缩包路径；项目本身是压缩包时可留空")},
                  {"max_entries", property("number", "最多返回条目数，默认 120")}}),
             objectSchema({{"entries", JsonValue::Object{{"type", "array"},
                                                         {"description", "压缩包条目摘要"}}},
                           {"safe_to_extract",
                            property("boolean", "是否未发现路径穿越、符号链接或嵌套压缩包")},
                           {"supported", property("boolean", "是否为当前 reader 可枚举格式")}})),
        spec("search_project_text", "在项目副本文本文件中搜索关键词，返回命中的路径和行号",
             ToolPermission::ReadProjectFiles,
             objectSchema({{"query", property("string", "要搜索的文本关键词")},
                           {"max_files", property("number", "最多扫描文件数量，默认 80")},
                           {"max_matches", property("number", "最多返回命中数量，默认 40")},
                           {"case_sensitive", property("boolean", "是否区分大小写，默认 false")}}),
             objectSchema({{"matches",
                            JsonValue::Object{{"type", "array"}, {"description", "命中行列表"}}}})),
        spec("draft_markdown_revision", "基于读取到的 Markdown 生成工作区修订稿，不覆盖原项目文件",
             ToolPermission::WriteWorkspace,
             objectSchema(
                 {{"path", property("string", "项目内 Markdown 相对路径")},
                  {"instruction", property("string", "修订目标")},
                  {"replacement_markdown", property("string", "可选，Brain 生成的完整替换稿")}}),
             objectSchema({{"workspace_path", property("string", "修订稿写入路径")},
                           {"preview", property("string", "修订稿预览")}})),
        spec("write_workspace_file", "把 Brain 生成的文本、代码、配置或报告写入会话工作区",
             ToolPermission::WriteWorkspace,
             objectSchema({{"path", property("string", "工作区内相对路径")},
                           {"content", property("string", "要写入的文本内容")}}),
             objectSchema({{"workspace_path", property("string", "写入后的工作区路径")},
                           {"format", property("string", "根据扩展名推断的输出格式")},
                           {"preview", property("string", "写入内容预览")}}))};
}

} // namespace cc
