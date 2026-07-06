# 现代 C++ 工程规范与中文注释规范

## 1. 技术栈限定

核心技术栈固定为：C++20 + CMake + Qt 6/QML + JSON 规则包 + Markdown/JSON 报告。

nlohmann/json 提供 JSON 值操作和从流解析 JSON 的常见用法，适合作为规则包和审计结果 JSON 的轻量依赖。来源：<https://github.com/nlohmann/json>

## 2. C++20 要求

CMake 必须启用：

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

## 3. 现代 C++ 规范

### 3.1 禁止裸 new/delete

禁止：

```cpp
auto* asset = new ProjectAsset();
delete asset;
```

推荐：

```cpp
auto asset = ProjectAsset{};
auto ptr = std::make_unique<ProjectAsset>();
```

### 3.2 必须使用 RAII

文件、临时目录、句柄、会话资源都必须由对象生命周期管理。

### 3.3 必须使用 std::filesystem

禁止字符串拼路径：

```cpp
auto path = root + "/" + filename;
```

必须：

```cpp
auto path = root / filename;
```

### 3.4 必须使用 Result<T>

可失败函数必须返回 `Result<T>`。

```cpp
template <typename T>
class Result {
public:
    static Result success(T value);
    static Result failure(std::string message);

    bool ok() const;
    const T& value() const;
    const std::string& error() const;
};
```

禁止失败时返回空对象掩盖错误。

### 3.5 必须使用 enum class

禁止裸 int 状态：

```cpp
int severity = 2;
```

必须：

```cpp
enum class Severity {
    Info,
    Warning,
    Blocker
};
```

### 3.6 const 正确性

不修改状态的方法必须标记 `const`。

```cpp
FormatResult detect(const std::filesystem::path& path) const;
```

### 3.7 禁止全局可变状态

禁止：

```cpp
std::vector<ProjectAsset> g_assets;
```

### 3.8 禁止魔法分数

禁止：

```cpp
score -= 17;
```

必须：

```cpp
score.addPenalty({
    .ruleId = "BIZ_MARKET_001",
    .points = 10,
    .reason = "市场规模声明缺少可追溯证据。"
});
```

## 4. 中文注释规范

### 4.1 总原则

本项目所有注释必须使用中文。中文注释解释“为什么”，不是重复“做什么”。

错误：

```cpp
// 遍历 assets
for (const auto& asset : assets) {}
```

正确：

```cpp
// 生成物和第三方依赖不能计入项目自主贡献，否则会高估项目真实实现能力。
for (const auto& asset : assets) {}
```

### 4.2 文件头注释

每个 `.hpp` 和 `.cpp` 必须有中文文件头注释。

```cpp
/**
 * @file InventoryEngine.hpp
 * @brief 构建竞赛项目材料包的资产语义清单。
 *
 * 本模块负责扫描项目目录，将文件识别为源码、商业计划书、市场调研、
 * 财务预测、成果证明、生成物、第三方依赖、敏感文件等项目资产角色。
 *
 * InventoryEngine 不负责判断项目是否通过审计，它只提供可信审计的基础数据。
 * 最终审计结论由 RuleEngine、EvidenceMatcher 和 AuditEngine 给出。
 */
```

### 4.3 类注释

每个公开类必须有中文注释。

```cpp
/**
 * @brief 项目资产语义识别引擎。
 *
 * 该类负责从 ProjectContext 中构建 ProjectInventory。
 * 它会结合文件扩展名、特殊文件名、路径语义、二进制检测和内容采样，
 * 判断每个文件在竞赛项目中的语义角色。
 *
 * 注意：本类不产生评分，不生成补证任务，不调用大模型。
 */
class InventoryEngine {
public:
    Result<ProjectInventory> build(const ProjectContext& context) const;
};
```

### 4.4 函数注释

公开函数必须有中文注释，包含 brief、参数、返回值、失败情况。

```cpp
/**
 * @brief 从项目上下文构建资产清单。
 *
 * @param context 已通过路径安全校验的项目上下文。
 * @return 成功时返回 ProjectInventory；失败时返回错误信息。
 *
 * 失败情况：
 * - 项目根目录不存在；
 * - 项目路径越过工作区边界；
 * - 文件数量超过限制；
 * - 文件系统权限不足。
 */
Result<ProjectInventory> build(const ProjectContext& context) const;
```

### 4.5 安全注释

涉及路径、解包、写文件、联网、LLM、shell、敏感文件的代码必须写中文安全注释。

```cpp
// 这里必须使用规范化路径检查，防止压缩包中的 ../../evil.txt
// 写出工作区目录，造成路径穿越漏洞。
if (!PathGuard::isInsideRoot(workspaceRoot, targetPath)) {
    return Result<void>::failure("压缩包条目越过工作区边界");
}
```

### 4.6 TODO 注释规范

禁止：

```cpp
// TODO: 实现
```

允许：

```cpp
// TODO(v2): 当前版本只对 PDF 做文件级证据识别，不抽取扫描件文字。
// 后续接入 OCR 后，可将扫描版证明材料纳入 EvidenceMatcher。
```

TODO 必须包含版本、原因、当前替代方案和后续方向。

## 5. AI 防废话代码规则

Codex 生成的任何代码必须满足：能编译、有真实输入、有真实输出、有错误处理、有测试、有中文注释、有模块边界、有安全约束、有可解释结果。不允许给简单代码复杂化


禁止生成：

```cpp
bool isGoodProject() {
    return true;
}
```

禁止生成：

```cpp
std::string advice() {
    return "建议进一步完善项目。";
}
```

所有建议必须具体到：规则 ID、风险原因、缺失证据、影响范围、补证材料、优先级。

## 6. 编译质量门禁

GCC / Clang：

```cmake
target_compile_options(contest_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
)
```

Debug 模式：

```cmake
target_compile_options(contest_core PRIVATE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
)

target_link_options(contest_core PRIVATE
    -fsanitize=address,undefined
)
```

MSVC：

```cmake
target_compile_options(contest_core PRIVATE
    /W4
    /permissive-
)
```

## 7. clang-format

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
DerivePointerAlignment: false
PointerAlignment: Left
SortIncludes: true
```

## 8. clang-tidy

```yaml
Checks: >
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -readability-magic-numbers

HeaderFilterRegex: 'include/cc/.*|src/.*'
```
