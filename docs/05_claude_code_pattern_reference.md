# Claude Desktop-style 竞赛工作流模式参考

## 1. 参考原则

本项目可以参考 Claude 桌面端的会话工作区流程和 agentic 产品中的运行时模式，但不能复制 Claude Code 或其他商业产品源码，不能使用泄露、反编译或未授权源码，也不能把项目评审做成脱离规则和证据的通用聊天机器人或通用代码助手。

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
| Tool Cards | StagedAuditPipeline / AgentRuntime | 工具调用以可见卡片展示，不允许自由乱跑 |
| Artifacts | ReportPreview / RepairPlan / AuditDiff | 报告、修复计划和差分作为可预览产物 |
| Tool Registry | ToolRegistry | 注册 inventory、audit、report、repair、diff 等竞赛工具 |
| Permissions | PermissionGate | 控制文件写入、命令执行、联网、LLM 调用 |
| Hooks | LifecycleHookManager | 在审计前后、修复前后和导出前后执行确定性检查 |
| Memory | PROJECT_RULES.md / project_rules.json | 保存赛道、项目规则、编码规范、竞赛约束和用户确认 |
| Subagents | Implicit Rule Matching | 商业、软件、科研、社会实践等差异由 JSON 规则包、证据状态和一致性结果隐式匹配 |
| Session Store | AuditSessionStore | 保存审计输入、规则版本、工具输出和报告 |
| Diff workflow | RepairDiff / repaired_project | 所有修复先生成计划和 diff，不覆盖原项目 |
| Context compaction | EvidenceSummary / AssetSummary | 长材料压缩为结构化摘要 |
| Permission modes | PermissionGate / accessMode | 高风险动作必须受权限模式约束 |

## 3. 禁止事项

禁止：

```text
1. 复制 Claude Code 或其他商业产品源码。
2. 使用泄露、反编译或未授权源码。
3. 在文档中写“基于 Claude Code 源码改造”。
4. 把项目判断做成脱离规则和证据的通用代码助手或通用聊天机器人。
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
- 右侧或可展开区域只展示 Agent 自动参考的上下文索引，包括资产清单、CPIR、声明—证据图、规则 findings、修复计划和 diff；审计报告不作为常驻板面模块；
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

- 审计步骤默认由 StagedAuditPipeline 决定；
- LLM 不能跳过 RuleEngine；
- LLM 不能直接写最终分数；
- LLM 不能直接生成通过结论；
- 所有工具输出必须进入 AuditSession；
- 所有风险项必须绑定规则 ID 或证据缺口；
- 会话工作区只能解释和组织已有审计结果，不能替代 C++ Core。

## 6. ToolRegistry

工具协议：

```cpp
struct AgentToolSpec {
    std::string name;
    std::string description;
    ToolPermission permission;
    JsonValue inputSchema;
    JsonValue outputSchema;
};

struct AgentToolCall {
    std::string id;
    std::string name;
    std::string reason;
    JsonValue input;
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

当前实现默认只允许安全读取项目副本和导出用户指定的报告。WriteWorkspace、NetworkAccess、LLMAccess 都需要当前任务的显式能力快照；本文件中的交互模式参考不得覆盖 `PermissionGate` 的实际默认策略。
默认禁止：ReadExternalFiles、ModifyOriginalProject、ExecuteCommand。

高风险动作必须用户确认：执行脚本、读取项目外文件、写入原项目、扫描超大文件、解包嵌套压缩包、导出最终提交版材料。联网和 LLM 默认允许，但没有 API key 时不得发起模型请求。

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

## 10. 隐式规则匹配

赛道差异不再通过显式 Analyzer 模块暴露给界面，而由 JSON 规则包、CPIR、证据匹配和一致性检查共同完成。

规则匹配约束：

- 每条规则必须保留 `rule_id`、严重度、失败原因和补证任务；
- 规则只能消费 ProjectInventory、CPIR、ProjectClaim、EvidenceMatch 和 ConsistencyIssue；
- Workbench 只展示匹配结果和工具观察，不展示“技能模块”；
- LLM Brain 可以先提出研判，但必须经过 AdvisoryReconciler 与确定性规则/证据校验；
- 任何规则匹配都不能绕过 RuleEngine、EvidenceMatcher 或 TrustScoreCalculator。

## 11. 竞赛可信智能体协作

类似 Claude 的协作体验应该表现为“竞赛可信智能体”：

- 解释某个风险项为什么出现；
- 告诉用户缺什么材料；
- 按 blocker、warning、提交截止风险排列优先级；
- 生成答辩可能被问到的问题；
- 根据已有 CPIR 和证据状态生成补证清单；
- 自动翻阅项目副本中的文本和 Markdown；
- 像工具循环一样根据每次 observation 决定下一步，而不是一次性规则匹配回复；
- 在工作区生成 Markdown 修订稿、修复计划、模板和报告。

它不应该表现为“通用助手”：

- 不自由聊天；
- 不脱离当前项目；
- 不自由执行命令；
- 不伪造材料；
- 不替代核心审计结论。

## 12. 最终要求

像 Claude Desktop-style 产品一样组织受控会话体验，但不要做 Claude 的复制品；要做面向大学生竞赛项目材料包的专用可信编译运行时。
