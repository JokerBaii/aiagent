# 安全模型、权限门控与修复边界

## 1. 安全目标

系统必须保证：

- 不直接修改原始项目；
- 不默认执行项目脚本；
- 不默认联网；
- 不默认调用 LLM；
- 不默认上传敏感文件；
- 解包压缩包时防止路径穿越；
- 所有高风险动作必须用户确认；
- 修复必须采用 diff-first 模式。

## 2. 工作区模型

所有输入和输出必须落在项目配置的工作区根下，默认是 `contest-compiler/.workspaces/<session_id>/`。不得把会话文件、修复产物、报告、缓存或临时项目写到仓库指定目录之外。

```text
contest-compiler/.workspaces/
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

默认允许：

```text
ReadProjectFiles
WriteWorkspace
ExportReport
```

默认禁止：

```text
ReadExternalFiles
ModifyOriginalProject
ExecuteCommand
```

NetworkAccess 和 LLMAccess 在底层任务快照中默认关闭。Workbench 检测到 endpoint、模型和 API key 组成的完整配置通过校验后，会自动为模型任务开启这两项能力，不再要求第二次确认；缺少有效配置或任务快照不匹配时不得向外部模型服务发起请求。

## 5. 高风险动作

以下动作必须用户确认：

1. 执行项目脚本；
2. 访问用户配置的 LLM endpoint 之外的网络；
3. 读取项目目录外文件；
4. 写入原始项目；
5. 扫描超过默认大小限制的大文件；
6. 解包嵌套压缩包；
7. 导出最终提交版报告。

调用用户已配置并通过校验的 LLM endpoint 不再单独弹出确认；删除或破坏配置会立即恢复本地模式。

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

敏感文件默认不得传给外部工具、网络接口或 LLM。

## 7. 禁止自动执行脚本

第一版禁止自动执行：

```text
npm install
pip install
bash start.sh
python app.py
cmake --build
docker compose up
```

如果后续加入可运行性检测，必须在隔离环境执行，并经过用户确认。

## 8. Hooks

内置 hooks：

| Hook | 作用 |
|---|---|
| PathSafetyHook | 检查路径是否越界 |
| SensitiveFileHook | 检查敏感文件是否外传 |
| NoOriginalOverwriteHook | 阻止覆盖原项目 |
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

禁止：

- 直接覆盖 original_project；
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
- 修复不得覆盖原项目；
- 报告不得隐藏 blocker；
- 上述仍需确认的高风险动作必须有中文提示和确认流程。
- 所有会话产物必须留在配置的工作区根内，不得写到项目指定目录外。
