# 全量功能需求规格说明

当前产品只保留 Qt/QML Workbench 作为用户入口。所有审计、补证、差分、报告导出和可选 LLM Brain 能力都通过 Workbench 会话工作区触发；C++ Core、Agent Runtime 和 LLM 模块保持可测试、可复核、可独立构建。

## 1. FR-01 项目材料包导入

系统应支持导入项目目录和常见压缩包，并建立隔离工作区。

输入：项目目录、`.zip`、`.tar`、`.tgz`、`.tar.gz`、`.gz`、`.7z` 等可安全解析的材料包。  
输出：ProjectContext、原始项目路径、工作区路径、解包状态、文件基础列表。

验收标准：

- 能扫描普通项目目录；
- 能识别隐藏文件；
- 能跳过 `.git` 等无关目录；
- 能标记嵌套压缩包；
- 解包时防止路径穿越；
- 不直接修改原始项目。

## 2. FR-02 项目资产语义识别 PASI

PASI，即 Project Asset Semantic Inventory。系统应识别每个文件的格式、类型和项目角色。

每个 ProjectAsset 必须包含：路径、文件名、扩展名、大小、格式、MIME、语言、角色、重要性、是否可审计、是否生成物、是否第三方、是否敏感、风险标记。

## 3. FR-03 竞赛类型识别

系统应根据用户选择和材料特征识别项目类型。

支持类型：商业创新、软件开发、工程产品、科研学术、社会实践、公益创业、电商三创、AI 应用、综合创新创业、未知。

输出：CompetitionTypeResult，包含类型、置信度、判断理由。

## 4. FR-04 CPIR 项目中间表示构建

CPIR 即 Competition Project Intermediate Representation。系统将不同类型项目统一转化为结构化项目对象。

字段包括：project_name、competition_type、track、target_user、pain_point、solution、product_or_service、technical_route、business_model、market_analysis、competitor_analysis、financial_projection、team_structure、current_results、social_value、claims、evidence_matches、risk_items。

如果材料不足，必须显式标记为空缺和风险，不允许编造。

## 5. FR-05 文本抽取

系统应从 `.md`、`.txt`、`.json`、`.yaml`、`.docx`、`.pptx`、`.xlsx`、`.pdf` 中抽取可审计文本。

第一版要求：

- Markdown / TXT 直接读取；
- JSON / YAML 抽取字符串值和结构键；
- docx / pptx / xlsx 使用 OpenXML 基础文本抽取；
- PDF 可抽取则抽取，不可抽取则标记 NEED_REVIEW；
- 不对扫描件内容进行幻觉解释。

## 6. FR-06 项目声明抽取

系统应从申报书、商业计划书、PPT、README、调研报告中抽取承诺性声明。

声明类型包括：UserTraction、MarketScale、TechnicalCapability、BusinessModel、Revenue、CostReduction、Patent、Copyright、Partnership、Prototype、ResearchResult、SocialImpact、Deployment、Unknown。

每条声明必须包含：claim_id、claim_text、claim_type、source_file、confidence、initial_risk。禁止凭空生成声明。

## 7. FR-07 声明和证据匹配

系统应判断项目声明是否有证据支撑。

状态包括：SUPPORTED、PARTIAL、UNSUPPORTED、CONFLICTED、NEED_REVIEW。

证据映射规则示例：

| 声明 | 证据 |
|---|---|
| 已有用户 | 用户数据、后台截图、问卷样本、访谈记录 |
| 市场规模大 | 行业报告、统计数据、TAM/SAM/SOM |
| 成本降低 | 成本测算表、对比实验 |
| 已有合作 | 合作协议、盖章证明、邮件记录 |
| 已申请专利 | 专利申请受理通知书 |
| 已实现功能 | 源码、演示视频、部署说明 |
| 已有营收 | 订单、合同、流水 |
| 方法有效 | 实验指标、baseline、消融实验 |

## 8. FR-08 材料一致性审计

系统应检查不同材料之间是否存在矛盾。

检查对象：PPT vs 申报书、商业计划书 vs 财务预测、市场调研 vs 用户画像、竞品分析 vs 差异化优势、团队介绍 vs 技术路线、成果证明 vs 项目声明、README vs 源码结构、技术路线 vs 部署文档、商业模式 vs 收入预测。

输出 ConsistencyIssue：issue_id、severity、description、affected_files、fix_suggestion。

## 9. FR-09 多赛道规则引擎

系统应支持 JSON 规则包，至少包括：common_rules、business_innovation_rules、software_project_rules、research_project_rules、social_practice_rules、ecommerce_rules。

规则输出 AuditFinding，必须包含 rule_id、severity、title、reason、evidence、fix_suggestion。

## 10. FR-10 可信评分与可信债务

评分维度：材料完整性 15、项目逻辑自洽性 15、声明证据匹配度 20、赛道规则匹配度 15、技术/商业/科研可行性 15、成果真实性 10、答辩风险控制 10。

输出：总分、可信债务、各维度分、扣分原因、关联规则、关联证据。禁止 AI 直接打分。

## 11. FR-11 补证任务生成

系统应根据风险项生成可执行补证任务。任务包含：task_id、title、priority、reason、required_material、affected_rules、related_files。

任务必须具体，不允许输出“建议进一步完善项目”。

## 12. FR-12 修复计划与材料重构

允许生成：README 模板、商业计划书结构建议、竞品分析表模板、财务预测表模板、市场调研补证清单、部署说明模板、答辩问题清单、风险分析章节、材料一致性修改建议。

禁止生成：虚假用户数量、虚假营收、虚假合作协议、虚假专利、虚假实验结果、虚假市场数据。

## 13. FR-13 二次审计与差分

系统应比较修复前后的 trust_score、trust_debt、blocker_count、warning_count、evidence_coverage、material_completeness、consistency_score、fix_task_status，并输出 AuditDiff。

## 14. FR-14 报告导出

系统应支持 Markdown 和 JSON 报告导出。

报告必须包含：项目概况、资产清单、竞赛类型、CPIR、声明证据匹配、材料一致性风险、赛道规则审计、可信评分、补证任务、修复计划、二次审计差分。

## 15. FR-15 Qt/QML Workbench

桌面端必须采用会话工作区流程，并实现：会话历史、项目拖拽导入、拖入即自动缺点评审、工具调用观察、资产清单展示、资产角色分布、可信评分仪表盘、blocker/warning 展示、声明证据匹配展示、材料一致性风险展示、补证任务展示、审计后询问是否优化、二次审计差分、Markdown/JSON 报告导出。

QML 只负责 UI，不允许承载核心业务逻辑。

## 16. FR-16 竞赛可信智能体协作

系统可以提供会话式智能体体验，并将其作为桌面端主交互流程；该能力必须绑定当前竞赛项目、审计会话、规则结果和工具轨迹。智能体作为“大脑”组织流程，确定性规则包、证据匹配和一致性检查作为裁决层，所有公开评审结果只抓缺点。

必须支持：

- 基于当前 AuditSession 解释 blocker、warning、可信债务和补证任务；
- 根据赛道、CPIR、证据覆盖率和规则结果生成下一步计划；
- Composer 主入口使用 `/audit`、`/agent <任务>`、`/task <任务>`、`/status`、`/compact`、`/clear`、`/help` 等命令；权限模式不作为主输入栏功能，而是在设置中管理；
- 普通自然语言输入必须作为常规问答或 agent task；当已有项目但尚未生成审计结果时，应先启动缺点评审；
- LLM Brain 必须按单步 decision 运行迭代工具循环：每一步基于已有 `AgentObservation` 决定继续调用一个工具或最终回答；
- 授权 LLM 后首次项目评审必须由 Brain 调用 `run_project_audit`，接收确定性审计各阶段观察和强类型结果后继续研判；不得由 Controller 绕过 Brain 直接启动本地流水线；
- 每轮 Brain decision 必须携带受限长度的用户/助手对话历史；Brain 调用失败时不得静默降级成本地回答；
- Brain 网络与工具循环必须离开 Qt UI 线程执行，使输入队列和界面状态在模型运行期间保持响应；
- 本地 AgentRuntime 负责执行注册工具、权限检查、路径边界和观察记录；未授权 LLM 时才使用本地观察摘要；
- 每轮智能体任务必须输出结构化 `AgentEvent` 和 JSON trace，Workbench 只能展示事件，不得在 Controller 中临时拼装工具轨迹；
- 通过 ToolRegistry 调用受控工具，不允许自由调用未注册工具；
- 在项目副本内枚举文件、搜索文本、读取文本/Markdown，并将 Markdown 修订稿、新模板或清单写入会话工作区；
- 高风险动作经过 PermissionGate、LifecycleHookManager 和 Workbench 设置中的权限模式；
- 审计完成后必须输出缺点评审报告，并询问用户是否优化项目；用户确认后只能把优化方案、补证清单和 diff-first 草稿写入安全工作区，默认不覆盖原项目；
- 对话输出必须引用规则 ID、证据来源、缺失材料或明确的审计上下文；
- 对话历史、用户确认和关键中间结果进入 AuditSessionStore 或 ProjectMemory；
- LLM 关闭时，核心审计、评分、补证任务和报告导出仍然可用。

禁止：

- 把项目评审做成脱离规则和证据的通用聊天应用；
- 复制第三方产品的品牌、图标、配色、文案或专有视觉资产；
- 脱离竞赛项目上下文给出项目评价、评分或优化结论；
- 让 LLM 决定最终评分或通过结论；
- 让 LLM 伪造商业数据、实验数据、合作证明、专利证明或用户规模；
- 让模型自由执行 shell；
- 直接覆盖原始项目文件。
