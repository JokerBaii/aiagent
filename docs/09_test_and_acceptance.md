# 测试与验收方案

当前项目只保留 Workbench 用户入口，验收围绕 C++ Core、Agent Runtime、可选 LLM、Qt/QML Workbench、报告导出和安全边界展开。

## 1. 单元测试

单元测试覆盖：

- loader：目录、单文件、zip、tar/libarchive、逐条软降级、路径穿越、目标冲突、压缩炸弹；
- inventory：扩展名与内容签名格式检测、未知格式、延迟资产、角色分类、生成物、第三方依赖、敏感文件；
- text：Markdown/TXT/JSON/YAML/OpenXML/PDF；
- cpir：竞赛类型识别和项目中间表示；
- claim：声明抽取；
- evidence：声明证据匹配；
- consistency：材料一致性；
- rules：规则包加载、校验、条件求值和规则触发；
- audit：可信评分、可信债务和差分；
- repair：补证任务、修复计划和 diff-first 边界；
- report：Markdown/JSON 导出；
- agent：权限、hooks、工具注册、项目记忆、会话存储；
- llm：endpoint 解析、响应解析、未授权阻断。

## 2. 构建验收

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

期望产物：

```text
build/debug/contest-workbench
build/debug/libcontest_core.a
build/debug/libcontest_agent.a
build/debug/libcontest_llm.a
build/debug/contest_tests
build/debug/contest_dependency_tests
```

## 3. Workbench 验收

Workbench 必须具备：

- 会话工作区首屏；
- 项目路径输入或拖拽；
- 受控工具调用卡片；
- 默认权限状态展示；
- artifact 列表；
- 资产清单、CPIR、声明证据、材料一致性、规则风险、可信评分、补证任务；
- 二次审计差分；
- Markdown/JSON 导出；
- 可选 LLM Brain 迭代工具循环；
- 授权 LLM 时首次导入由 Brain 调用 `run_project_audit`，并在下一步收到确定性阶段观察和强类型审计结果；
- Brain 回合携带用户/助手历史，在工作线程运行且失败时不静默切换本地回答；
- 未授权 LLM 时本地 AgentRuntime 仍可执行受控文件工具；
- 交互式工具至少支持项目文件枚举、项目文本搜索、文本/Markdown 读取、Markdown 工作区修订和工作区文本产物写入；
- Composer slash command 路由：`/audit`、`/agent <任务>`、`/task <任务>`、`/help`；
- 普通自然语言输入作为 agent task，不做关键词命令识别；
- AgentRuntime / BrainAgentLoop 输出 `AgentEvent` 和包含 events 的 JSON trace；
- 授权 LLM 时 Brain 每步基于 `AgentObservation` 决定继续调用工具或最终回答，未授权时保守降级为本地观察摘要；
- Workbench 会话展示来自 AgentRuntime 事件，而不是 Controller 自行拼装工具轨迹；
- LLM 缺少 API key 或运行时授权标志时不发起模型请求。

## 4. 安全验收

必须验证：

- 解包前检查路径穿越；
- 不跟随符号链接、不自动展开嵌套或加密条目，但保留其安全元数据；
- 一个超限或暂不支持的文件不导致整个目录或归档失败；
- 路径穿越、重复目标、文件/目录冲突、压缩炸弹和损坏归档仍必须拒绝并回滚；
- 不直接修改原始项目；
- 修复只生成计划和 diff；
- 默认拒绝联网和 LLM 权限；只有当前任务明确授权且存在有效 API key 时才能发起模型请求；
- OpenXML、PDF 和压缩包解析不调用 shell 或外部工具；
- 报告不生成虚假数据；
- LLM 工具决策不参与最终评分。

## 5. 质量门禁

```bash
./tools/acceptance.sh
./tools/quality.sh
```

`acceptance.sh` 负责 fresh configure/build、单元测试、模块归属、Workbench 结构和安全边界检查。  
`quality.sh` 负责 clang-format、clang-tidy、ASan/UBSan 构建和测试。

## 6. Definition of Done

1. CMake 配置成功
2. `contest_core` 编译成功
3. `contest_agent` 编译成功
4. `contest_llm` 编译成功
5. `contest-workbench` 编译成功
6. 单元测试通过
7. Workbench 能运行可信编译并展示结果
8. Workbench 能导出 Markdown/JSON 报告
9. Workbench 能展示二次审计差分
10. 默认权限阻断联网和 LLM
11. 报告中每个关键结论包含规则 ID 或证据来源
12. 关掉 LLM 后系统仍能完成核心审计
13. 无直接覆盖原项目逻辑
14. 无 AI 伪造数据逻辑
15. 质量脚本通过
