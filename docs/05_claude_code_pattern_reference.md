# Claude Desktop-style 竞赛工作流模式参考

## 1. 参考原则

本项目可以参考 Claude 桌面端的会话工作区流程和 agentic 产品中的运行时模式，但不能复制 Claude Code 或其他商业产品源码，不能使用泄露、反编译或未授权源码，也不能把项目做成无项目上下文的通用聊天机器人或通用代码助手。

Claude Code Agent SDK 文档显示，其能力包含 built-in tools、hooks、subagents、MCP、permissions、sessions 等。来源：<https://code.claude.com/docs/en/agent-sdk/overview>

Claude Code hooks 文档描述了 hooks 的事件、配置 schema、JSON 输入输出、退出码等机制。来源：<https://code.claude.com/docs/en/hooks>

这些能力在本项目中的定位是：

```text
参考工作流，不参考产品目标。
参考权限和工具边界，不复制源码。
参考协作体验，不做通用 agent。
```

## 2. 架构映射

| Claude Desktop-style 概念 | 本项目对应实现 | 说明 |
|---|---|---|
| Conversation Workspace | ContestSessionWorkspace | 会话是主入口，但必须绑定竞赛项目 |
| Sidebar Sessions | AuditSessionStore | 左侧展示会话历史、项目空间和导出记录 |
| Tool Cards | AuditPipeline / RepairPipeline | 工具调用以可见卡片展示，不允许自由乱跑 |
| Artifacts | ReportPreview / RepairPlan / AuditDiff | 报告、修复计划和差分作为可预览产物 |
| Tool Registry | ToolRegistry | 注册 inventory、audit、report、repair、diff 等竞赛工具 |
| Permissions | PermissionGate | 控制文件写入、命令执行、联网、LLM 调用 |
| Hooks | LifecycleHookManager | 在审计前后、修复前后和导出前后执行确定性检查 |
| Memory | PROJECT_RULES.md / project_rules.json | 保存赛道、项目规则、编码规范、竞赛约束和用户确认 |
| Subagents | SpecializedAnalyzers | 商业、软件、科研、社会实践、证据、一致性、安全等专用 analyzer |
| MCP / External Tools | ExternalToolAdapter | 预留搜索、OCR、GitHub、浏览器等外部工具适配，但默认禁用 |
| Session Store | AuditSessionStore | 保存审计输入、规则版本、工具输出和报告 |
| Diff workflow | RepairDiff / repaired_project | 所有修复先生成计划和 diff，不覆盖原项目 |
| Context compaction | EvidenceSummary / AssetSummary | 长材料压缩为结构化摘要 |
| Human approval | HumanApprovalGate | 高风险动作必须用户确认 |

## 3. 禁止事项

禁止：

```text
1. 复制 Claude Code 或其他商业产品源码。
2. 使用泄露、反编译或未授权源码。
3. 在文档中写“基于 Claude Code 源码改造”。
4. 把本项目做成通用代码助手或无项目上下文的通用聊天机器人。
5. 让模型自由执行 shell 命令。
6. 让模型直接覆盖项目文件。
7. 让模型决定最终审计结论或可信评分。
8. 让模型伪造市场数据、用户数量、营收、合作、专利或实验结果。
9. 复制 Claude 的品牌、图标、配色、文案或专有视觉资产。
```

正确表述：

```text
本项目参考 agentic coding 工具的通用工作流思想，结合大学生竞赛项目材料包审计场景，
设计面向项目可信编译的专用审计运行时。
系统重点不在通用代码生成，而在项目资产识别、材料一致性审计、证据匹配、规则评估、可信评分和补证闭环。
```

## 4. 桌面端交互骨架

桌面端流程应高度效仿 Claude 桌面端的会话体验：

- 左侧是会话历史、项目空间、最近材料包和导出记录；
- 中央是连续会话流，展示用户问题、系统计划、工具调用卡片、审计结论和后续建议；
- 底部是 composer，支持拖拽材料包、选择赛道、输入追问、触发快捷命令；
- 右侧或可展开区域展示 artifacts，包括资产清单、CPIR、声明—证据图、规则 findings、修复计划、审计报告和 diff；
- 高风险动作使用确认弹窗或会话内确认卡片；
- 审计、修复和导出过程必须可暂停、可解释、可复核。

视觉层面必须原创，不复制 Claude 的品牌、图标、配色、文案或专有视觉资产。

## 5. 受控 Conversation Loop

通用 Agent Loop：

```text
User Goal
↓
Plan
↓
Select Tool
↓
Run Tool
↓
Observe Result
↓
Update State
↓
Repeat or Finish
```

本项目受控 Conversation Loop：

```text
New / Existing Session
↓
Attach Project Package
↓
Select Competition Track
↓
Show Audit Plan
↓
Inventory Tool
↓
Text Extraction Tool
↓
CPIR Builder Tool
↓
Claim Extractor Tool
↓
Evidence Matcher Tool
↓
Consistency Checker Tool
↓
Rule Engine Tool
↓
Trust Score Tool
↓
Fix Task Generator Tool
↓
Repair Planner Tool
↓
Reporter Tool
↓
Report / Repair Artifact
↓
Follow-up Questions
```

关键限制：

- 工具执行顺序默认由 AuditPipeline 决定；
- LLM 不能跳过 RuleEngine；
- LLM 不能直接写最终分数；
- LLM 不能直接生成通过结论；
- 所有工具输出必须进入 AuditSession；
- 所有风险项必须绑定规则 ID 或证据缺口；
- 会话工作区只能解释和组织已有审计结果，不能替代 C++ Core。

## 6. ToolRegistry

工具接口：

```cpp
class ITool {
public:
    virtual ~ITool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual ToolPermission requiredPermission() const = 0;
    virtual Result<ToolResult> run(const ToolInput& input,
                                   AuditSession& session) const = 0;
};
```

必须注册：

```text
inventory_project
extract_text
build_cpir
extract_claims
match_evidence
check_consistency
run_rules
calculate_trust_score
generate_fix_tasks
generate_repair_plan
export_markdown_report
export_json_report
verify_diff
explain_audit_finding
generate_defense_questions
```

所有工具都必须有明确权限、输入 schema、输出 schema 和错误处理，不允许注册“万能 shell 工具”。

## 7. PermissionGate

权限类型：

```cpp
enum class ToolPermission {
    ReadProjectFiles,
    ReadExternalFiles,
    WriteWorkspace,
    ModifyOriginalProject,
    ExecuteCommand,
    NetworkAccess,
    LLMAccess,
    ExportReport
};
```

默认允许：ReadProjectFiles、WriteWorkspace、ExportReport。  
默认禁止：ModifyOriginalProject、ExecuteCommand、NetworkAccess、LLMAccess。

高风险动作必须用户确认：执行脚本、访问网络、调用 LLM、读取项目外文件、写入原项目、扫描超大文件、解包嵌套压缩包、导出最终提交版材料。

## 8. Hooks

HookPoint：

```cpp
enum class HookPoint {
    BeforeProjectLoad,
    AfterProjectLoad,
    BeforeInventory,
    AfterInventory,
    BeforeAudit,
    AfterAudit,
    BeforeRepairPlan,
    AfterRepairPlan,
    BeforeReportExport,
    AfterReportExport
};
```

内置 hooks：PathSafetyHook、SensitiveFileHook、NoOriginalOverwriteHook、RulePackValidationHook、ReportCompletenessHook、NoFabricatedEvidenceHook。

Hook 失败必须阻断当前流程，不允许忽略。

## 9. Project Memory

初始化项目时生成：

```text
.project-trust/
  PROJECT_RULES.md
  project_rules.json
  permissions.json
  hooks.json
```

PROJECT_RULES.md 保存项目赛道、必需材料、禁止无证据声明、报告要求、用户确认过的权限和补证约束。

## 10. Specialized Analyzers

必须实现：BusinessAnalyzer、SoftwareAnalyzer、ResearchAnalyzer、SocialPracticeAnalyzer、EvidenceAnalyzer、ConsistencyAnalyzer、SecurityAnalyzer、ReportAnalyzer。

Analyzer 接口：

```cpp
class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;
    virtual std::string name() const = 0;
    virtual bool supports(CompetitionType type) const = 0;
    virtual Result<std::vector<AuditFinding>> analyze(
        const CPIR& cpir,
        const ProjectInventory& inventory,
        const EvidenceGraph& evidenceGraph
    ) const = 0;
};
```

Analyzer 只补充赛道专用审查，不允许绕过 RuleEngine、EvidenceMatcher 或 TrustScoreCalculator。

## 11. 竞赛顾问式协作

类似 Claude 的协作体验应该表现为“竞赛顾问”：

- 解释某个风险项为什么出现；
- 告诉用户缺什么材料；
- 按 blocker、warning、提交截止风险排列优先级；
- 生成答辩可能被问到的问题；
- 根据已有 CPIR 和证据状态生成补证清单；
- 在用户确认后输出修复计划、模板和报告。

它不应该表现为“通用助手”：

- 不自由聊天；
- 不脱离当前项目；
- 不自由执行命令；
- 不伪造材料；
- 不替代核心审计结论。

## 12. 最终要求

像 Claude Desktop-style 产品一样组织受控会话体验，但不要做 Claude 的复制品；要做面向大学生竞赛项目材料包的专用可信编译运行时。
