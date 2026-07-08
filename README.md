# Contest Project Trust Compiler

竞赛项目可信编译器是一个面向大学生竞赛项目材料包的离线可信审计工具。它把项目目录或压缩材料包转化为资产清单、CPIR、声明、证据匹配、规则风险、可信评分、补证任务和 Markdown/JSON 报告。

本项目当前只保留 Qt/QML Workbench 作为用户入口。桌面端界面和流程采用会话工作区、侧边栏、composer、工具调用卡片、artifact 预览、权限确认、项目记忆和 diff-first 修复体验；这些能力都服务于竞赛项目审计、补证、二次审计和报告导出。

## 目标边界

- C++ Core 负责全部审计逻辑。
- `contest_agent` 负责 ToolRegistry、PermissionGate、Hooks、ProjectMemory、Session 和 StagedAuditPipeline 编排。
- `contest_core` 不链接 LLM/OpenSSL；可选 Brain 单独由 `contest_llm` 承载。
- Qt/QML Workbench 负责展示、授权、导出和会话交互。
- 智能体协作由 LLM Brain 迭代决策并驱动受控工具，可翻阅项目副本、读取文本/代码/配置、检查压缩包条目、生成任意扩展名的工作区文本产物；未启用 Brain 时只做本地上下文诊断，不冒充模型回答。
- JSON 规则包负责多赛道规则配置。
- JSON 解析/序列化由 `nlohmann/json` 承担。
- OpenXML 内部 XML 文本解析由 `pugixml` 承担；`Catch2` 覆盖固定第三方技术栈 smoke test。
- 未配置 API key 时系统不联网、不调用 LLM、不执行项目脚本、不覆盖原项目。
- LLM Brain 是可选模块，支持从环境变量或 Workbench 配置读取 endpoint/model/key；key 只进入运行时内存，不写入代码、报告或交付包。
- 报告不生成虚假用户、营收、合作、专利、实验结果或市场数据。
- 会话可以作为主操作入口，但必须绑定竞赛项目和 AuditSession；不把 LLM 输出作为最终评分。

## 构建

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

可执行文件：

```text
build/debug/contest-workbench
```

## 桌面端

启动：

```bash
./build/debug/contest-workbench
```

Workbench 支持会话工作区首屏、输入或拖拽项目路径、运行可信编译、展示项目上下文和会话历史、查看受控工具调用卡片、查看权限状态和 artifact 列表、查看评分和 blocker/warning 数量、查看资产/CPIR/声明证据/一致性/风险/补证任务、展示二次审计差分、通过 API key 让 LLM Brain 运行迭代工具循环，并导出 Markdown/JSON 报告。核心业务逻辑不在 QML 或 Controller 中实现。

会话流和操作布局按 `mistydew/tokenicode-deepseek-alpha` 的实际组件结构用 QML 复刻：TOKEN/CODE 风格侧栏、New Task/Add Project、会话列表、右侧 files/preview/skills 面板、底部 composer、slash command popover、权限模式按钮、模型状态和主题设置。复刻的是开源交互结构和布局逻辑，不复制品牌专有素材。

主题配置与参考仓库对齐：`appearance`、`colorTheme`、`backgroundTheme`、`fontPreset`、`uiFontSize` 持久化到 Qt Settings；默认值为 `light`、`black`、`garden`、`microsoft`、`18`。Workbench 内置 garden、sakura、lake、dusk、ink、vscode、minimal 背景皮肤。

会话流采用 Codex/Claude Code-style 的内联渲染：用户消息、审计计划、工具调用观察、智能体回复、系统提示和产物各有独立样式；工具卡片直接内联在对话流中展开。左侧栏是会话与项目（新建会话、当前项目、查看报告入口），分析结果（可信仪表盘、资产、画像、证据、一致性、风险、补证、差分、报告导出）不再作为常驻标签页，而是作为对话里可打开的报告 artifact 抽屉。底部 composer 支持多行输入、`/audit`、`/ask`、`/plan`、`/code`、`/bypass`、`/agent`、`/help` 命令快捷项，Enter 发送、Shift+Enter 换行。

项目导入支持原生目录选择器和窗口拖拽目录/材料包；访问模式与 tokenicode/Claude Code 风格一致：`ask` 为默认沙箱问答，`plan` 只生成计划，`code` 在受控工作区执行，`bypass` 才允许完全授权能力。

可信审计以真实分步流水线驱动：`StagedAuditPipeline` 在受控 Hook 和路径边界内逐步执行整理材料、读取文本、判断赛道、生成画像、抽取声明、匹配证据、检查一致性、执行规则、计算评分、生成补证任务和修复计划，每一步都把真实中间结果流式回报到会话流，不再使用界面层伪造的进度。步骤序列的唯一真相源在 `contest_core` 的 `StagedAuditEngine`，批处理入口 `AuditEngine` 与会话入口共享同一序列。

授权 LLM 后支持混合研判：LLM 先基于审计上下文给出风险判断和评分建议，`AdvisoryReconciler` 用确定性规则和证据逐条校验，命中规则/证据的判为已印证、无依据的判为待核实、与确定性结论矛盾的判为冲突并降级；最终可信评分仍取 `TrustScoreCalculator` 的确定性结果，LLM 建议评分只用于对比解释。

智能体会话能力必须绑定 AuditSession、规则结果、证据状态和补证任务。它可以调用注册工具翻阅项目副本、搜索文本、读取材料、读取源码、检查压缩包/代码包条目、生成工作区产物，不得绕过权限模式读取项目外文件、伪造材料或计算最终评分。

Composer 使用显式命令协议：`/audit` 运行可信审计，`/ask`、`/plan`、`/code`、`/bypass` 切换模式，`/ask <任务>`、`/code <任务>`、`/bypass <任务>` 会先切换模式再提交任务，`/agent <任务>` 或 `/task <任务>` 直接提交智能体任务，`/help` 查看命令；其他自然语言输入一律作为 agent task，不再通过关键词判断命令。

每次智能体任务都会形成结构化 turn：`AgentPlan`、单步 Brain decision、工具调用、`AgentObservation`、`AgentEvent` 和 JSON trace。授权 LLM 时，Brain 会在每一步基于 observations、工具 schema、文件格式元数据和当前权限模式决定继续调用一个工具或最终回答；未授权或 Brain 调用失败时只输出本地上下文诊断。Workbench 只展示这些事件，不在 Controller 中拼装工具轨迹。

LLM 环境变量：

```bash
export ANTHROPIC_AUTH_TOKEN="..."
export ANTHROPIC_BASE_URL="https://example.com"
export ANTHROPIC_MODEL="claude-sonnet-4-6"

export OPENAI_API_KEY="..."
export OPENAI_BASE_URL="https://api.openai.com"
export OPENAI_MODEL="gpt-4o-mini"
```

Anthropic 配置优先；未提供 Anthropic key 时使用 OpenAI-compatible 配置。`*_BASE_URL` 可以是 base URL，也可以直接是 `/v1/messages` 或 `/v1/chat/completions` endpoint。检测到 key 后 Workbench 会自动启用 Brain 运行时；界面只显示掩码，不把 env key 回显到 QML 文本框，仍会展示权限模式和工具 trace。

## 输入安全

支持项目目录、`.zip` 以及 libarchive 可解析的 `.tar/.tgz/.tar.gz/.gz/.7z`。压缩包导入会先检查路径穿越、符号链接和嵌套压缩包，再解包到 `.workspaces/<session_id>/input/`。

zip 读取由 C++/ZLIB 的 `ZipArchiveReader` 完成，不调用 shell 或外部 `unzip`。非 zip 压缩包由 `LibArchiveReader` 调用 libarchive 解析，不执行压缩包内任何内容。PDF 文本抽取由 `PdfContentStreamParser` 保守解析已有内容流，不调用 `pdftotext`；扫描件或复杂编码会显式标记为 NEED_REVIEW。目录导入也会先复制到隔离工作区，审计只读取隔离工作区，不直接扫描或修改原始项目。

## 验收

```bash
./tools/acceptance.sh
./tools/quality.sh
```

脚本会执行：

- CMake configure/build；
- 单元测试；
- clang-format dry-run；
- clang-tidy 核心模块检查；
- AddressSanitizer / UBSan Debug 构建和单测；
- Workbench 会话页结构检查；
- 模块边界检查；
- 固定依赖 smoke test。

需求落实矩阵见 `docs/REQUIREMENT_AUDIT.md`。

## 打包

```bash
./tools/package_release.sh
```

输出 TGZ 包位于项目根目录，包含 Workbench、规则包、示例项目、tools、docs 和 README。

## 规则包

规则文件位于 `rules/`：

- `common_rules.json`
- `business_innovation_rules.json`
- `software_project_rules.json`
- `research_project_rules.json`
- `social_practice_rules.json`
- `ecommerce_rules.json`

每条规则必须包含 `rule_id`、中文说明、触发条件、失败原因和补证任务。

## Agent Brain 安全边界

- 未配置 key 时不联网、不调用 LLM；检测到 env/UI key 时自动进入 Brain 模式；
- API key 不写入审计报告或交付包；
- Brain 负责逐步生成结构化工具决策，AgentRuntime 负责权限检查、路径边界、格式识别和工具执行；
- Brain 可在授权后读取审计摘要、rule_id、风险项、证据状态、补证任务和项目副本中的文本、Markdown、源码、配置和压缩包目录；
- Markdown 修订默认写入会话工作区，不覆盖原项目；
- Brain 不能推翻 RuleEngine、EvidenceMatcher 和 TrustScoreCalculator 的最终结果；
- 混合研判中 LLM 可先给出风险判断和评分建议，但每条研判都要经 `AdvisoryReconciler` 与确定性规则/证据校验，冲突项降级并标注；最终可信评分仍由确定性 TrustScoreCalculator 裁决，LLM 建议评分只用于对比解释。

## 当前可复核能力

- 项目目录、zip 和 libarchive 支持压缩包导入；
- 目录导入复制到隔离工作区并记录 ProjectContext/input_files/unpack_status；
- PASI 资产语义识别；
- 资产 MIME、语言、重要性、生成物、第三方和敏感标记导出；
- 敏感文件、生成物、第三方依赖识别；
- Markdown/TXT/CSV/源码文本抽取；
- JSON/YAML 结构键和值抽取；
- docx/pptx/xlsx OpenXML 基础文本抽取，复用内部 ZipArchiveReader 读取 XML；
- OpenXML XML 节点文本由 pugixml 解析；
- PDF 可抽取文本读取，扫描件标记 NEED_REVIEW；
- 竞赛类型识别；
- CompetitionTypeResult 置信度和判断理由；
- CPIR 构建；
- 声明抽取；
- 声明和证据匹配；
- 材料一致性审计；
- JSON 规则引擎；
- 可信评分和可信债务；
- 补证任务和修复计划；
- diff-first 修复产物；
- Markdown/JSON 报告；
- 二次审计 diff；
- AuditEngine、EvidenceGraph、AuditSessionStore 和 ProjectMemory/project_rules.json；
- `contest_agent` 独立 target，agentic runtime 不编译进 `contest_core`；
- Business/Software/Research/SocialPractice/Evidence/Consistency/Security/Report 专用 Analyzer 注册；
- Anthropic 和 OpenAI-compatible 可选 LLM Brain，支持 env/UI key、endpoint/model 配置、自动 env 启用和工具循环 trace；
- 权限和 Hook 清单；
- Workbench 会话工作区首屏，展示 tokenicode-style 侧栏、项目上下文、会话历史、composer、slash commands、受控工具卡片、权限卡片、artifact 预览入口、Agent Brain 运行结果、主题配置和工具 trace。
