# 竞赛项目智能工作台 Markdown 文档包

项目名称：竞赛项目可信编译器  
英文名称：Contest Project Trust Compiler  
桌面端名称：Contest Trust Workbench  
技术栈：C++20 + CMake + Qt 6/QML + JSON 规则包 + Markdown/JSON 报告  
目标：构建一个严谨、现代、可解释、可测试、可复核的大学生竞赛项目材料包可信审计与自动补证系统。

## 产品定位

本项目专注于大学生竞赛项目材料包，不做通用 Claude 克隆，不做通用聊天机器人，也不做通用代码助手。

桌面端界面和流程应采用 Claude Desktop-style 会话工作区，包括侧边栏、会话主流程、composer、工具调用卡片、artifact 预览、权限确认、项目记忆、上下文压缩、专用 analyzer 和 diff-first 修复流程。所有这些能力都必须服务于竞赛项目审计、补证、修复计划、二次审计和报告导出。

一句话原则：

```text
竞赛是主业务，Claude Desktop-style 是界面流程和交互范式。
```

## 文档清单

| 文件 | 用途 |
|---|---|
| `01_research_and_requirements.md` | 正式调研与需求分析文档 |
| `02_functional_requirements.md` | 全量功能需求规格说明 |
| `03_architecture_and_directory_tree.md` | 严谨现代架构与目录树 |
| `04_ultimate_codex_goal.md` | 可直接用于 Codex Goal 模式的最终工程提示词 |
| `05_claude_code_pattern_reference.md` | Claude Desktop-style 工作流模式参考，不复制源码 |
| `06_cpp_engineering_and_comment_style.md` | 现代 C++ 工程规范与中文注释规范 |
| `07_rule_pack_spec.md` | 多赛道 JSON 规则包设计规范 |
| `08_security_model.md` | 安全模型、权限门控、hooks、修复边界 |
| `09_test_and_acceptance.md` | 单元测试、验收标准与 Definition of Done |
| `10_implementation_roadmap.md` | 完整实现顺序与交付路线 |
| `11_competition_agentic_workbench.md` | Claude Desktop-style 竞赛智能工作台定位 |
| `12.md` | 强制模块化与禁止单文件实现规则 |

## 使用方式

1. 先读 `01_research_and_requirements.md` 和 `11_competition_agentic_workbench.md`，确认项目不是通用 AI 助手。
2. 将 `04_ultimate_codex_goal.md` 作为 Codex `/goal` 的主提示词。
3. 将 `03_architecture_and_directory_tree.md`、`06_cpp_engineering_and_comment_style.md` 和 `12.md` 固定为工程约束。
4. 将 `07_rule_pack_spec.md` 作为规则包实现依据。
5. 将 `09_test_and_acceptance.md` 作为每轮实现后的检查清单。
6. 不允许跳过 C++ Core 直接做 QML，也不允许让 LLM 直接决定最终评分。

## 项目最终原则

```text
规则优先于生成
竞赛优先于通用对话
证据优先于判断
审计优先于修复
补证优先于编造
安全优先于自动化
核心优先于界面
测试优先于演示
可解释优先于炫技
```
