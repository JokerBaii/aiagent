# md 需求落实矩阵

本文件用于把当前 Workbench-only 目标映射到实现、测试和验收脚本。后续新增功能必须同步更新本矩阵，避免只看单个 README 或只做演示壳。

## 需求来源

- `README.md`
- `apps/contest-workbench/qml/pages/SessionWorkspacePage.qml`
- `tools/acceptance.sh`
- `tools/quality.sh`

## 功能需求映射

| 来源 | 要求 | 当前实现 | 验证入口 |
|---|---|---|---|
| FR-01 | 目录和 zip/tar 等压缩包导入，隔离工作区，不改原项目 | `ProjectLoader`、`ArchiveExtractor`、`ZipArchiveReader`、`LibArchiveReader`、`PathGuard`，目录和压缩包都落到 `.workspaces/<session>/input` | `tests/loader/LoaderTests.cpp` |
| FR-02 | PASI 资产语义识别 | `FormatDetector`、`RoleClassifier`、`SensitiveFileDetector`、`GeneratedVendoredDetector`、`InventoryEngine` | `tests/inventory/InventoryTests.cpp` |
| FR-03 | 竞赛类型识别含置信度和理由 | `CompetitionTypeDetector`、`CompetitionTypeResult` | `tests/cpir/CpirTests.cpp` |
| FR-04 | CPIR，不足字段显式标记 | `CPIRBuilder` | `tests/cpir/CpirTests.cpp` |
| FR-05 | md/txt/json/yaml/OpenXML/pdf 文本抽取 | `PlainTextExtractor`、`StructuredTextExtractor`、`OpenXmlTextExtractor`、`PdfTextExtractor`、`PdfContentStreamParser`、`TextExtractionService` | `tests/text/TextTests.cpp` |
| FR-06 | 声明抽取，禁止凭空生成 | `ClaimExtractor` | `tests/claim/ClaimTests.cpp` |
| FR-07 | 声明和证据匹配 | `EvidenceMatcher`、`EvidenceGraph` | `tests/evidence/EvidenceTests.cpp` |
| FR-08 | 材料一致性审计 | `ConsistencyChecker` | `tests/consistency/ConsistencyTests.cpp` |
| FR-09 | 多赛道 JSON 规则引擎 | `RulePackLoader`、`RulePackValidator`、`RuleConditionEvaluator`、`RuleEngine`，六个规则包 | `tests/rules/*` |
| FR-10 | 可信评分和可信债务，禁止 AI 打分 | `TrustScoreCalculator`，LLM Brain 独立于 core | `tests/audit/AuditTests.cpp`，acceptance 模块归属检查 |
| FR-11 | 具体补证任务 | `FixTaskGenerator` | `tests/repair/RepairTests.cpp` |
| FR-12 | 修复计划和 diff-first 边界 | `RepairPlanner`、`RepairDiff`，只生成计划和 diff 文本 | `tests/repair/RepairTests.cpp` |
| FR-13 | 二次审计差分 | `DiffVerifier`、`AuditDiff` JSON | `tests/audit/AuditTests.cpp` |
| FR-14 | Markdown/JSON 报告 | `MarkdownReporter`、`JsonReporter` | `tests/report/ReportTests.cpp` |
| FR-15 | Qt/QML Workbench | `CompileController` 桥接，`SessionWorkspacePage` 作为首屏展示项目上下文、会话历史、composer、受控工具卡片、权限卡片、artifact 列表、资产、评分、风险、证据、差分和导出入口 | CMake 构建 `contest-workbench`，acceptance 检查会话页、toolCards、permissionCards、artifacts 和 sessionHistory 绑定 |
| FR-16 | 竞赛可信智能体协作 | `contest_agent` 管理结构化工具、权限、hooks、会话、slash command 路由、AgentEvent/trace、文件翻阅、项目文本搜索、Markdown 工作区修订和工作区文本产物写入；`contest_llm` 运行可选 Brain 迭代工具循环，解析单步 decision，并基于 observations 决定继续调用工具或最终回答 | `tests/agent/AgentTests.cpp`、`tests/llm/LlmTests.cpp` |

## 架构和模块边界

| 要求 | 当前落实 |
|---|---|
| C++ Core 负责可信审计逻辑 | `contest_core` 包含数据模型、loader、inventory、text、cpir、claim、evidence、consistency、rules、audit、repair、report |
| Agentic runtime 独立 | `contest_agent` 包含 `ToolRegistry`、`PermissionGate`、`LifecycleHookManager`、`ProjectMemory`、`AuditSessionStore`、`AgentRuntime`、`StagedAuditPipeline` 和专用 Analyzer |
| LLM 只能可选主控 | `contest_llm` 单独承载 `LlmBrain`、`BrainAgentLoop`、HTTPS 客户端、endpoint/parser 和工具 decision 解析；`contest_core` 不包含 LLM/OpenSSL 对象，最终评分不受 LLM 改写 |
| QML Controller 只能桥接 | `CompileController` 调用 core/agent/llm/report 服务并转成 QML 模型，不实现扫描、规则、评分、证据匹配 |
| 禁止单文件和万能 Manager | `include/cc/<module>/` 与 `src/<module>/` 拆分；没有 `all_in_one/app/core/manager` 等禁止命名 |

## 安全和权限

| 要求 | 当前落实 | 验证入口 |
|---|---|---|
| 路径穿越防护 | `PathGuard` 校验 root 内路径，解包前检查条目 | `tests/loader/LoaderTests.cpp` |
| 不覆盖原项目 | 目录输入先复制到 `.workspaces/<session>/input`，修复只生成计划/diff | `tests/loader/LoaderTests.cpp`、`tests/repair/RepairTests.cpp` |
| zip/libarchive/OpenXML/PDF 不执行 shell | `ZipArchiveReader`、`LibArchiveReader`、`OpenXmlTextExtractor` 和 `PdfContentStreamParser` 都不调用外部工具 | acceptance 禁止 `popen/std::system/unzip/pdftotext` 出现在相关产品代码 |
| 默认不联网、不调用 LLM | `PermissionGate` 默认拒绝 `NetworkAccess` 和 `LLMAccess` | `tests/agent/AgentTests.cpp` |
| LLM 显式授权 | `LlmBrain` 必须同时有联网授权、LLM 授权和 API key | `tests/llm/LlmTests.cpp` |
| hooks 阻断 | `LifecycleHookManager` 内置 PathSafety、SensitiveFile、NoOriginalOverwrite、RulePackValidation、ReportCompleteness、NoFabricatedEvidence | `tests/agent/AgentTests.cpp` |
| 敏感文件识别 | `SensitiveFileDetector` 标记 `.env`、key、token、credentials 等 | `tests/inventory/InventoryTests.cpp` |

## 测试和验收

| 门禁 | 当前入口 |
|---|---|
| CMake configure/build | `cmake --preset debug && cmake --build --preset debug` |
| 单元测试 | `ctest --preset debug --output-on-failure` |
| 端到端验收 | `./tools/acceptance.sh` |
| 质量门禁 | `./tools/quality.sh` |
| 打包 | `./tools/package_release.sh` |
| 模块归属 | acceptance 检查 `contest_core` 不含 agent/LLM 对象，`contest_agent` 和 `contest_llm` 各含对应对象 |
| 报告可追溯 | Markdown/JSON 保留 rule_id、证据来源、补证任务、评分扣分原因 |
| 固定技术栈 | `nlohmann/json` 负责 JSON 解析/序列化，`pugixml` 负责 OpenXML XML 文本解析，`Catch2` 负责固定依赖 smoke test，ZLIB 负责 zip/PDF deflate 解压，libarchive 负责非 zip 压缩包读取，OpenSSL 只在 `contest_llm` |
