# 实现路线图

当前路线图以 Workbench-only 产品形态为准。所有用户可见流程都进入 Qt/QML Workbench，核心能力保持在 C++ 模块中。

## 1. Milestone 1：工程骨架

交付：

- CMakePresets；
- `contest_core`；
- `contest_agent`；
- `contest_llm`；
- `contest-workbench`；
- `contest_tests`；
- clang-format / clang-tidy；
- README 与 docs。

验收：能编译空核心和 Workbench 壳，但不能留下空壳业务函数。

## 2. Milestone 2：Core 数据模型

交付 Result、ProjectContext、ProjectAsset、ProjectInventory、CPIR、Claim、EvidenceMatch、AuditFinding、TrustScore、FixTask、RepairPlan、AuditDiff、AuditSession。

## 3. Milestone 3：安全导入

交付 ProjectLoader、ArchiveExtractor、ZipArchiveReader、LibArchiveReader、PathGuard。

验收：目录、zip、tar/tgz 可导入；路径穿越、符号链接、嵌套压缩包被拒绝。

## 4. Milestone 4：资产和文本

交付 FormatDetector、RoleClassifier、SensitiveFileDetector、GeneratedVendoredDetector、InventoryEngine、TextExtractionService、OpenXML/PDF 基础抽取。

## 5. Milestone 5：CPIR 和声明证据

交付 CompetitionTypeDetector、CPIRBuilder、ClaimExtractor、EvidenceMatcher、EvidenceGraph、ConsistencyChecker。

## 6. Milestone 6：规则、评分和修复

交付 RulePackLoader、RulePackValidator、RuleConditionEvaluator、RuleEngine、TrustScoreCalculator、FixTaskGenerator、RepairPlanner、RepairDiff。

## 7. Milestone 7：报告和差分

交付 MarkdownReporter、JsonReporter、DiffVerifier。

## 8. Milestone 8：Agent Runtime

交付 ToolRegistry、PermissionGate、LifecycleHookManager、ProjectMemory、AuditSessionStore、AgentRuntime、StagedAuditPipeline。赛道专用判断由 JSON 规则包隐式匹配，不再新增显式 analyzer 模块。

## 9. Milestone 9：可选 LLM Brain

交付 EndpointParser、HttpResponseParser、HttpsJsonClient、LlmBrain、BrainAgentLoop。

验收：无显式授权时必须阻断；LLM 输出不进入最终评分。

## 10. Milestone 10：Workbench

交付 CompileController、AuditResultModels、WorkbenchSessionModels、QML pages、资源注册、会话工作区首屏、导出流程和二次审计差分流程。

验收：Workbench 能展示项目上下文、受控工具观察、设置式权限边界、artifact、资产、CPIR、风险、补证任务、报告和差分。

## 11. Milestone 11：质量和打包

交付 acceptance、quality、package_release。

验收：

- `./tools/acceptance.sh` 通过；
- `./tools/quality.sh` 通过；
- `./tools/package_release.sh` 生成只包含 Workbench、规则包、示例、docs 和 tools 的包。

## 12. 最终交付物

1. 可运行 Workbench
2. 完整 C++ Core
3. 完整 Agent Runtime
4. 可选 LLM Brain
5. 规则包
6. 示例坏案例
7. 单元测试
8. 验收脚本
9. README 和 docs
10. 可分发 TGZ 包
