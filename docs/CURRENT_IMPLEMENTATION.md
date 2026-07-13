# 当前实现状态

更新日期：2026-07-13

本文件描述仓库当前代码，而不是最初设想。判断功能是否存在时，优先级依次为：自动测试、产品代码、本文件、需求与路线文档。

## 当前可运行入口

- 唯一用户入口是 Qt 6/QML 桌面程序 `contest-workbench`。
- `CompileController` 只负责异步任务和 QML 数据转换，确定性检查由 C++ 服务完成。
- 项目导入后，`StagedAuditPipeline` 依次执行材料整理、文本读取、赛道判断、项目画像、声明提取、证据匹配、一致性检查、规则执行、评分和修改任务生成。
- LLM 是可选的联网辅助能力。endpoint、模型和密钥通过校验后自动启用，没有额外确认开关；缺少有效配置时本地规则检查仍可完整运行。
- 每次请求仍携带内部网络/LLM 能力快照；Workbench 只在有效 LLM 配置存在时自动放行模型任务。工作区写入继续由访问模式控制。
- 修订只写入会话工作区或 repaired project，不覆盖原始项目。
- 检查结果页采用统一的暖白/深色响应式样式；得分直接显示为“80 分”而非比例或装饰竖线。资产可点击预览，差分和报告导出提供原生文件选择器，小窗口不会强制挤压双栏，技术 trace 默认收起。

## Target 边界

| Target | 当前职责 |
|---|---|
| `contest_core` | loader、inventory、text、cpir、claim、evidence、consistency、rules、audit、repair、report |
| `contest_agent` | 工具注册、权限、hooks、会话、项目记忆、分步审计编排、受控文件策略和工具执行 |
| `contest_llm` | Provider 配置、HTTPS、响应解析、Brain 决策循环和建议校验 |
| `contest-workbench` | 桌面交互、任务调度、结果模型和导出入口 |

## Agent Runtime 当前拆分状态

`AgentRuntime` 仍是重点技术债务。2026-07-12 开始按职责迁移，禁止再把新的工具实现直接堆入 `AgentRuntime.cpp`。

已拆出：

- `AgentFilePolicy`：UTF-8 清洗、安全截断、文本/二进制判断、敏感路径和敏感内容识别。
- `AgentPermissionPolicy`：把每轮请求的能力快照转换为单次工具授权结果。
- `AgentTraceSerializer`：统一 call、observation、event 和完整回合 trace JSON，Brain 循环与本地运行时不再各自拼装。
- `ToolRegistry`：工具规格与权限声明。
- `PermissionGate`：权限判定。
- `WorkspaceEditor`：受控工作区写入。
- `StagedAuditPipeline`：确定性审计阶段编排。

下一批拆分顺序：

1. `AgentPathResolver`：project/workspace 路径解析、资产映射和符号链接边界。
2. `AgentProjectTools`：列文件、检查文件、读取文本、搜索文本和检查压缩包。
3. `AgentAuditTools`：运行审计、读取审计结果、二次审计。
4. `AgentWorkspaceTools`：生成修订稿和写入安全副本。
5. `AgentToolDispatcher`：只做工具名到 handler 的注册和分发。

目标边界是让 `AgentRuntime` 最终只保留：请求生命周期、权限门控、调用 handler、聚合强类型结果和取消传播。

## 智能体循环完成条件

- 首次项目评判必须先执行确定性审计，模型不能直接给结论。
- `/optimize` 会启用强制完善工作流：必须在 repaired project 产生真实文本变更，并完成修改后确定性复审和 `AuditDiff`，才允许最终回答。
- 产生修改前必须至少成功读取一份相关项目文件；模型不能只根据文件名、摘要或猜测直接写入。
- 模型提前回答时，运行时会把未完成条件作为 observation 回灌并继续循环。
- 下一步模型请求优先保留最近的工具 observation、完整工具输入 schema 和用户目标；大体积审计背景排在其后。客户端不再用 16/96 KiB 小窗口裁剪长上下文，并会先对历史、观察、审计条目和长字段做结构化压缩，避免模型看不到 `path` 等必填参数。
- LLM 模型 ID 不使用本地白名单或固定 DeepSeek 下拉项；用户/环境可显式提供模型，也可按 endpoint 与凭证动态读取 provider 的 `data[].id` 模型目录后选择。endpoint 协议决定 Anthropic 或 OpenAI-compatible 请求和认证方式，未显式设置的 temperature 不会写入请求。多家环境凭证并存时必须用 `LLM_PROVIDER` 选择；完整配置校验通过后自动启用联网模型。
- 参数校验失败会把工具 input schema 回灌并允许模型继续纠正；只有已经成功执行的完全相同调用才进入重复熔断。
- 本地审计会话不会直接拼接规则字段：缺失项保留为结构化数据，`NEED_REVIEW_*` 等抽取状态会翻译成用户能理解的原因；同一规则产生的发现和补证任务会合并表达，避免报告腔和重复句子。
- 每轮最大工具步数来自 `LlmConfig.maxAgentSteps`（默认 32，合法范围 1–256）；任何工作流达到上限但没有模型最终回答时都返回失败，不再用自动总结伪装成成功。
- 普通自然语言不会被关键词猜成写入操作；只有显式 `/optimize` 或 Code/扩展读取模式允许写入隔离工作区，任何模式都不修改原项目、不执行 shell。
- 没有配置联网智能辅助服务时，本地诊断会明确拒绝 `/optimize`，不会把“只看了上下文”显示成“已经完成修改”。
- 修改前基线由循环持有独立快照，二次审计结果覆盖当前结果后不会产生悬空指针。

## 变更同步规则

新增或改变能力时，同一个提交必须至少更新以下一项：

- `docs/REQUIREMENT_AUDIT.md` 中的功能与测试映射；
- 本文件的当前状态或 Runtime 拆分状态；
- 对应模块的自动测试。

路线图和早期需求文档可以保留历史背景，但不能用将来时描述为当前已实现能力。
