# 严谨现代架构与目录树

## 1. 架构原则

系统采用“C++ Core + Agent Runtime + Optional LLM + Qt/QML Workbench + JSON Rule Packs + Markdown/JSON Report”的分层结构。

核心原则：

```text
C++ Core 负责可信逻辑
Agent Runtime 负责受控工具、权限、hooks、会话和项目记忆
Qt/QML Workbench 负责可视化展示、授权、交互和导出
JSON 负责规则配置
Markdown/JSON 负责报告输出
LLM 只作为可选辅助，不负责最终裁决
Workbench UI 采用会话工作区，但业务逻辑不进 QML
```

## 2. 顶层架构

```text
Project Package
    ↓
ProjectLoader / ArchiveExtractor / PathGuard
    ↓
PASI: AssetInventoryEngine
    ↓
TextExtractor
    ↓
CompetitionTypeDetector
    ↓
CPIRBuilder
    ↓
ClaimExtractor
    ↓
EvidenceMatcher
    ↓
ConsistencyChecker
    ↓
RuleEngine
    ↓
TrustScoreCalculator
    ↓
FixTaskGenerator / RepairPlanner
    ↓
DiffVerifier
    ↓
MarkdownReporter / JsonReporter
    ↓
CompileController
    ↓
Qt/QML Workbench
```

## 3. Agentic Runtime 层

```text
ToolRegistry
PermissionGate
LifecycleHookManager
ProjectMemory
SpecializedAnalyzers
AgentRuntime
StagedAuditPipeline
AuditSessionStore
DiffWorkflow
```

这些模块服务于竞赛审计流水线。它们负责组织工具调用、权限确认、审计会话、项目记忆和专用 analyzer，不直接替代 RuleEngine、EvidenceMatcher、TrustScoreCalculator 等确定性核心模块。

Agentic Runtime 的输入必须来自 ProjectContext、ProjectInventory、CPIR、EvidenceGraph、AuditResult 或用户明确确认；输出必须进入 AuditSession、RepairPlan、AuditDiff 或 Report，不允许生成脱离项目证据链的结论。

## 4. 现代目录树

```text
contest-compiler/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── .clang-format
├── .clang-tidy
├── docs/
│   └── REQUIREMENT_AUDIT.md
├── include/
│   └── cc/
│       ├── core/
│       ├── loader/
│       ├── inventory/
│       ├── text/
│       ├── cpir/
│       ├── claim/
│       ├── evidence/
│       ├── consistency/
│       ├── rules/
│       ├── audit/
│       ├── repair/
│       ├── report/
│       ├── agent/
│       ├── llm/
│       └── util/
├── src/
│   ├── core/
│   ├── loader/
│   ├── inventory/
│   ├── text/
│   ├── cpir/
│   ├── claim/
│   ├── evidence/
│   ├── consistency/
│   ├── rules/
│   ├── audit/
│   ├── repair/
│   ├── report/
│   ├── agent/
│   ├── llm/
│   └── util/
├── apps/
│   └── contest-workbench/
│       ├── main.cpp
│       ├── CompileController.cpp
│       ├── AuditResultModels.cpp
│       ├── WorkbenchSessionModels.cpp
│       ├── resources.qrc
│       └── qml/
├── rules/
├── examples/
├── tests/
└── tools/
```

## 5. 核心库划分

| Target | 作用 |
|---|---|
| `contest_core` | 数据模型、审计核心、规则、评分、报告 |
| `contest_agent` | ToolRegistry、PermissionGate、Hooks、Session |
| `contest_llm` | 可选 LLM Brain、API 客户端和建议生成，不进入 core |
| `contest-workbench` | Qt/QML 桌面端 |
| `contest_tests` | 单元测试 |

## 6. CMake Target 设计

```cmake
add_library(contest_core STATIC ...)
add_library(contest_agent STATIC ...)
add_library(contest_llm STATIC ...)
add_executable(contest-workbench apps/contest-workbench/main.cpp)
```

`contest-workbench` 可以实现侧边栏、会话流、composer、工具调用卡片、artifact 预览和权限确认界面，但只能依赖 `contest_core`、`contest_agent`、可选 `contest_llm` 和 UI Controller，不得把业务逻辑写进 QML。

如果桌面端使用 LLM Brain，必须通过显式授权的桥接服务调用 `contest_llm`，不得在 QML 中拼接提示词、读取敏感文件或计算审计结论。

## 7. 数据流

```text
ProjectContext
  → ProjectInventory
  → ExtractedTextCorpus
  → CompetitionTypeResult
  → CPIR
  → ProjectClaims
  → EvidenceMatches
  → ConsistencyIssues
  → AuditFindings
  → TrustScore
  → FixTasks
  → RepairPlan
  → AuditDiff
  → Reports
```

## 8. 分层约束

- `core` 不依赖 Qt。
- `core` 不依赖网络。
- `core` 不依赖 LLM。
- QML 不直接读写项目文件。
- `rules` 不调用 UI。
- `report` 不修改审计结果。
- `repair` 不覆盖原项目。
- `agent` 必须通过 PermissionGate 执行高风险动作。
