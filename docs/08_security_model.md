# 安全模型、权限门控与修复边界

## 1. 安全目标

系统必须保证：

- 导入、解包、确定性审计和 `RepairPlanner` 不修改原始项目；
- 智能体的文件写入、Shell/Bash、外部读取、网络和 LLM 能力由当前权限模式和单次任务能力快照共同约束；
- Plan 模式只生成计划，不执行写入、命令、网络或 LLM 工具；
- 没有有效 DeepSeek 配置时不调用 LLM；
- 非完全访问模式下阻断敏感路径和疑似密钥内容；
- 解包压缩包时防止路径穿越；
- 确定性修复计划采用 diff-first 模式，最终评分始终由确定性规则产生。

## 2. 工作区模型

确定性审计工作区根由 `CONTEST_WORKSPACE_ROOT` 指定；Workbench 未收到该环境变量时，使用 Qt 系统应用数据目录下的 `workspaces/`。审计会话可在其中建立 `<session_id>/input/` 分析副本。已有审计结果时，智能体使用 `<session_id>/agent/` 保存 `repaired-project`、补丁和工作区产物；审计前的任务回退到所选项目下的 `.project-trust/agent-workspace/`。完全访问模式下的原项目写入不属于工作区产物。

```text
<CONTEST_WORKSPACE_ROOT>/
  <session_id>/
    input/
    repaired/
    inventory.json
    cpir.json
    claims.json
    evidence.json
    audit.json
    fix_tasks.json
    repair_plan.md
    report.md
```

## 3. 路径安全

必须实现 PathGuard。

功能：

- 规范化路径；
- 检查目标路径是否在 root 内；
- 阻止 `../../evil.txt`；
- 阻止绝对路径写入；
- 阻止危险符号链接；
- 阻止解包覆盖已有文件。

示例中文注释：

```cpp
// 这里必须使用规范化路径检查，防止压缩包中的 ../../evil.txt
// 写出工作区目录，造成路径穿越漏洞。
if (!PathGuard::isInsideRoot(workspaceRoot, targetPath)) {
    return Result<void>::failure("压缩包条目越过工作区边界");
}
```

## 4. 权限模型

权限类型：

```cpp
enum class ToolPermission {
    ReadProjectFiles,
    ReadExternalFiles,
    WriteWorkspace,
    ModifyOriginalProject,
    ExecuteCommand,
    NetworkAccess,
    LLMAccess,
    ExportReport
};
```

`PermissionGate` 的基础默认允许：

```text
ReadProjectFiles
WriteWorkspace
ExportReport
```

`PermissionGate` 的基础默认禁止：

```text
ReadExternalFiles
ModifyOriginalProject
ExecuteCommand
```

Workbench 当前提供两种用户可见模式：

| 模式 | 行为 |
|---|---|
| 完全访问（默认） | 允许工作区写入、外部读取、原项目写入和 Shell/Bash；有效 DeepSeek 配置存在时允许 LLM 调用。网络能力也随完全访问任务快照开启。 |
| Plan | 只生成计划；阻断工作区写入、外部读取、原项目写入、命令、网络和 LLM 工具。 |

底层每次调用仍由 `AgentPermissionPolicy` 根据任务快照构造 `PermissionGate`。DeepSeek 请求必须同时具备有效 endpoint/model/API key、`NetworkAccess` 和 `LLMAccess`；任一条件缺失都不得发起模型请求。

## 5. 高风险动作

完全访问模式是对原项目写入、项目外读取、Shell/Bash 和一般网络能力的显式授权边界，当前不会为每次工具调用重复弹窗。因此只应对可信项目或已有版本控制/备份的目录启用。Plan 模式用于在执行前预览方案。

调用用户已配置并通过校验的 DeepSeek endpoint 不再单独弹出确认；删除或破坏配置会立即恢复无模型模式。归档解析仍受导入限额、路径边界和事务回滚保护，嵌套或加密归档默认只保留元数据。

DeepSeek 原生工具调用仍由 AgentRuntime 执行，不能绕过 PermissionGate、LifecycleHookManager 和 Workbench 权限模式。

## 6. 敏感文件识别

必须标记：

```text
.env
.env.local
*.pem
*.key
id_rsa
token.*
secret.*
credentials.*
包含 password/token/key 字段的配置文件
```

Plan 和其他非完全访问请求会阻断敏感路径及疑似密钥内容。完全访问模式显式放宽这层智能体文件策略，因此用户必须自行确认项目内容是否适合交给所配置的模型端点；审计报告仍不得主动写出密钥。

## 7. 命令执行边界

`execute_shell_command` 只在完全访问任务快照中注册为可执行能力，并在所选项目上下文中运行。Plan 模式必须拒绝该工具。完全访问不提供逐命令确认、固定超时或输出上限，因此不得用于来源不明的项目；需要先审阅时应使用 Plan。

## 8. Hooks

内置 hooks：

| Hook | 作用 |
|---|---|
| PathSafetyHook | 检查路径是否越界 |
| SensitiveFileHook | 检查敏感文件是否外传 |
| NoOriginalOverwriteHook | 保护确定性修复和工作区写入路径；经 `ModifyOriginalProject` 授权的专用原项目写入工具另行受控 |
| RulePackValidationHook | 检查规则包字段完整性 |
| ReportCompletenessHook | 检查报告是否包含评分、风险、补证任务 |
| NoFabricatedEvidenceHook | 阻止生成虚假数据和证据 |

Hook 失败必须阻断流程。

## 9. 修复边界

允许：

- 生成修复计划；
- 生成模板；
- 生成 `repaired_project/`；
- 生成 `repair.diff`；
- 重新审计 repaired_project。

确定性修复链路禁止：

- 由 `RepairPlanner` 直接覆盖 original_project；
- 伪造用户数据；
- 伪造营收；
- 伪造合作协议；
- 伪造专利；
- 伪造实验结果；
- 伪造市场报告来源。

## 10. 安全验收标准

- 路径穿越测试必须通过；
- `.env` 检测必须通过；
- 无有效配置或任务快照不允许时，联网必须被阻止；
- 无有效配置或任务快照不允许时，LLM 调用必须被阻止；
- 导入、确定性审计和 `RepairPlanner` 不得覆盖原项目；Plan 模式不得调用原项目写入工具；
- 报告不得隐藏 blocker；
- 完全访问/Plan 状态必须在界面中可见，切换后权限卡片与实际任务快照一致；
- 工作区工具不得越过各自配置的根路径，原项目写入只能走具备 `ModifyOriginalProject` 权限的专用工具。
