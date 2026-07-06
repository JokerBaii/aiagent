# Contest Project Trust Compiler

竞赛项目可信编译器是一个面向大学生竞赛项目材料包的离线可信审计工具。它把项目目录或压缩材料包转化为资产清单、CPIR、声明、证据匹配、规则风险、可信评分、补证任务和 Markdown/JSON 报告。

本项目当前只保留 Qt/QML Workbench 作为用户入口。桌面端界面和流程采用会话工作区、侧边栏、composer、工具调用卡片、artifact 预览、权限确认、项目记忆和 diff-first 修复体验；这些能力都服务于竞赛项目审计、补证、二次审计和报告导出。

## 目标边界

- C++ Core 负责全部审计逻辑。
- `contest_agent` 负责 ToolRegistry、PermissionGate、Hooks、ProjectMemory、Session 和薄 AuditPipeline 编排。
- `contest_core` 不链接 LLM/OpenSSL；可选 Brain 单独由 `contest_llm` 承载。
- Qt/QML Workbench 负责展示、授权、导出和会话交互。
- 竞赛顾问式协作只解释审计结果、追问缺失材料和组织补证计划，不替代 Core 裁决。
- JSON 规则包负责多赛道规则配置。
- JSON 解析/序列化由 `nlohmann/json` 承担。
- OpenXML 内部 XML 文本解析由 `pugixml` 承担；`Catch2` 覆盖固定第三方技术栈 smoke test。
- 系统默认不联网、不调用 LLM、不执行项目脚本、不覆盖原项目。
- LLM Brain 是可选插件，必须在 Workbench 中显式填入 API key 并授权联网和 LLM 调用。
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

Workbench 支持会话工作区首屏、输入或拖拽项目路径、运行可信编译、展示项目上下文和会话历史、查看受控工具调用卡片、查看权限状态和 artifact 列表、查看评分和 blocker/warning 数量、查看资产/CPIR/声明证据/一致性/风险/补证任务、展示二次审计差分、通过 API key 生成可选 LLM Brain 建议，并导出 Markdown/JSON 报告。核心业务逻辑不在 QML 或 Controller 中实现。

如果加入竞赛顾问式会话能力，它只能基于 AuditSession、规则结果、证据状态和补证任务解释下一步，不得自由执行命令、读取项目外文件、伪造材料或计算最终评分。

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

输出 TGZ 包位于项目根目录，包含 Workbench、规则包、示例项目、tools、docs、README 和 LICENSE。

## 规则包

规则文件位于 `rules/`：

- `common_rules.json`
- `business_innovation_rules.json`
- `software_project_rules.json`
- `research_project_rules.json`
- `social_practice_rules.json`
- `ecommerce_rules.json`

每条规则必须包含 `rule_id`、中文说明、触发条件、失败原因和补证任务。

## LLM Brain 安全边界

- 默认不联网、不调用 LLM；
- API key 不写入审计报告或交付包；
- Brain 只读取审计摘要、rule_id、风险项、证据状态和补证任务；
- Brain 不读取敏感文件正文，不覆盖原项目，不生成最终评分；
- Brain 输出只能作为解释和补证优先级建议，不能推翻 RuleEngine、EvidenceMatcher 和 TrustScoreCalculator 的结果。

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
- AuditEngine、EvidenceGraph、ExternalToolAdapter、AuditSessionStore 和 ProjectMemory/project_rules.json；
- `contest_agent` 独立 target，agentic runtime 不编译进 `contest_core`；
- Business/Software/Research/SocialPractice/Evidence/Consistency/Security/Report 专用 Analyzer 注册；
- OpenAI-compatible 可选 LLM Brain，支持 API key、endpoint/model 配置和显式授权；
- 权限和 Hook 清单；
- Workbench 会话工作区首屏，展示项目上下文、会话历史、composer、受控工具卡片、权限卡片、artifact 预览入口和顾问式补证摘要。
