# Ultimate Codex Goal

## 0. 项目精神

本项目不是普通课程设计，不是 AI 套壳工具，也不是代码生成器。本项目要实现一个严谨、现代、可解释、可测试、可复核的工程系统：竞赛项目可信编译器。

项目目标是帮助学生把一个看起来完整的项目，变成材料完整、逻辑自洽、证据充分、规则匹配、可提交、可答辩、可复核的可信竞赛项目。

Workbench 会话工作区是唯一用户入口。它承载工具调用、权限确认、项目记忆、上下文压缩和 diff-first 修复方式，但产品核心必须始终是竞赛项目材料包。

必须始终遵守：

```text
规则优先于生成
证据优先于判断
审计优先于修复
补证优先于编造
安全优先于自动化
核心优先于界面
测试优先于演示
可解释优先于炫技
```

## 1. 可直接使用的 Codex `/goal` Prompt

```text
/goal 实现完整的 Contest Project Trust Compiler。

技术栈固定为 C++20 + CMake + Qt 6/QML。

系统只保留 Qt/QML Workbench 作为用户入口：
1. C++ Core 负责全部可信审计逻辑。
2. Agent Runtime 负责受控工具、权限、hooks、项目记忆和审计会话。
3. Qt/QML Workbench 负责可视化工作台、授权、交互和报告导出。
4. JSON 规则包负责多赛道规则配置。
5. Markdown/JSON 负责报告导出。
6. LLM 只能作为可选插件，不能作为最终裁决者。

系统必须支持：
- 项目目录导入；
- zip 项目包安全解包；
- tar/tgz/tar.gz/gz/7z 等常见材料包安全解包；
- 项目资产语义识别 PASI；
- 文件格式检测；
- 文件角色分类；
- 竞赛类型识别；
- CPIR 项目中间表示；
- 项目声明抽取；
- 项目声明证据匹配；
- 材料一致性审计；
- 多赛道 JSON 规则引擎；
- 可信评分；
- 可信债务；
- blocker / warning 风险项；
- 补证任务生成；
- 修复计划生成；
- diff-first 修复模式；
- 二次审计差分；
- Markdown 报告导出；
- JSON 审计结果导出；
- Qt/QML 桌面工作台；
- 竞赛可信智能体协作面板；
- 单元测试；
- 中文注释；
- 中文文档；
- 中文报告。

允许参考的模式包括：
- tool registry；
- permission gate；
- hooks；
- project memory；
- implicit rule matching；
- session store；
- diff-first workflow；
- human-in-the-loop；
- context compaction。

禁止：
- 把项目评审做成脱离规则和证据的通用聊天机器人；
- 做成无项目上下文的项目评分应用；
- 复制第三方产品的品牌、图标、配色、文案或专有视觉资产；
- 让 LLM 决定最终评分；
- 让 LLM 生成虚假数据；
- 生成空壳函数；
- 写只有 TODO 的代码；
- 输出无意义建议；
- 直接覆盖原项目文件。

当需求存在不确定性时，先实现保守的规则化版本，写清限制，补充测试，不允许编造能力。
```

## 2. 项目最终定位

中文名：竞赛项目可信编译器  
英文名：Contest Project Trust Compiler  
桌面端：大学生项目材料审计平台

一句话定位：面向大学生竞赛项目材料包的智能审计、证据补证与可信报告工作台。

定位边界：竞赛是主业务。所有 agent、LLM、工具调用和 UI 协作能力都必须绑定竞赛项目、审计会话、规则结果和证据链。

## 3. 固定技术栈

- C++20；
- CMake；
- Qt 6；
- QML；
- Qt Quick Controls；
- nlohmann/json；
- libarchive；
- Catch2；
- pugixml；
- OpenSSL；
- Markdown / JSON 报告。

## 4. 必须实现的模块

```text
ProjectLoader
ArchiveExtractor
PathGuard
FormatDetector
RoleClassifier
InventoryEngine
TextExtractor
CompetitionTypeDetector
CPIRBuilder
ClaimExtractor
EvidenceMatcher
ConsistencyChecker
RuleEngine
TrustScoreCalculator
AuditEngine
FixTaskGenerator
RepairPlanner
DiffVerifier
MarkdownReporter
JsonReporter
ToolRegistry
PermissionGate
LifecycleHookManager
ProjectMemory
RulePackMatcher
AuditSessionStore
AgentRuntime
StagedAuditPipeline
CompileController
QML Pages
```

## 5. QML 完成要求

桌面端必须能：

- 使用会话工作区作为主流程；
- 展示会话历史、项目上下文、工具调用观察、设置式权限边界和报告 artifact；
- 拖拽导入项目；
- 显示项目资产清单；
- 显示资产角色分布；
- 显示可信评分；
- 显示 blocker / warning；
- 显示项目声明证据匹配；
- 显示材料一致性风险；
- 显示补证任务；
- 显示二次审计差分；
- 导出 Markdown/JSON 报告；
- 提供竞赛可信智能体会话能力，用于解释风险、追问材料、翻阅项目副本、生成补证优先级和答辩问题。

禁止把会话做成无项目上下文的普通聊天，禁止用 QML 写核心业务逻辑。会话界面只能读取 C++ Core 和 Agentic Runtime 输出的审计状态，不能直接扫描文件、执行规则、计算评分或拼接最终报告。

## 6. 中文注释要求

所有公开类、公开函数、安全边界、规则判断、评分扣分、证据匹配、修复限制、权限判断、hook 阻断必须有中文注释。

中文注释必须解释“为什么”，不是重复代码。

## 7. 现代 C++ 要求

- 必须使用 C++20；
- 禁止裸 new/delete；
- 必须使用 RAII；
- 必须使用 `std::filesystem::path`；
- 可失败函数必须返回 `Result<T>`；
- 必须 const-correct；
- 禁止全局可变状态；
- 禁止魔法分数；
- 必须使用 `enum class`；
- 必须通过 clang-format、clang-tidy 和单元测试。

## 8. Definition of Done

项目完成标准：

```text
1. CMake 配置成功
2. contest_core 编译成功
3. contest_agent 编译成功
4. contest_llm 编译成功
5. contest-workbench Qt/QML 编译成功
6. 单元测试通过
7. examples 下至少 4 类坏案例可审计
8. 每类案例能输出 blocker/warning
9. 能生成 Markdown 报告
10. 能生成 JSON 审计包
11. QML 能展示核心审计结果
12. 所有公开类和公开函数有中文注释
13. 安全相关逻辑有中文注释
14. 无裸 new/delete
15. 无空壳 TODO 函数
16. 无直接覆盖原项目逻辑
17. 无 AI 伪造数据逻辑
18. 报告中每个关键结论包含规则 ID 或证据来源
19. 关掉 LLM 后系统仍能完成核心审计
```

## 9. 实现顺序

```text
1. CMake 工程骨架
2. Core 数据模型
3. Result<T>
4. PathGuard
5. ProjectLoader
6. ArchiveExtractor
7. FormatDetector
8. RoleClassifier
9. InventoryEngine
10. TextExtractor
11. CompetitionTypeDetector
12. CPIRBuilder
13. ClaimExtractor
14. EvidenceMatcher
15. ConsistencyChecker
16. RuleEngine
17. TrustScoreCalculator
18. AuditEngine
19. FixTaskGenerator
20. RepairPlanner
21. DiffVerifier
22. MarkdownReporter
23. JsonReporter
24. ToolRegistry
25. PermissionGate
26. LifecycleHookManager
27. AuditSessionStore
28. 单元测试补齐
29. QML Controller
30. QML 页面
31. 竞赛可信智能体协作面板
32. README 与 docs
```
