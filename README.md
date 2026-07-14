# 大学生项目材料审计平台

一套已经完整实现、可直接构建运行的大学生竞赛项目材料审计与完善工作台。平台把项目目录、单份材料或压缩包转换为资产清单、项目画像、声明—证据关系、一致性问题、赛道规则风险、可信评分、补证任务和可导出的审计报告。

当前版本：`0.1.0`<br>
技术栈：C++20、CMake、Qt 6/QML、JSON Rule Packs<br>
运行入口：`contest-workbench`

![Workbench 新任务界面](docs/images/workbench-home.png)

## 已完成功能

- 导入项目目录、任意单文件，以及 ZIP、TAR、TGZ、GZ、BZ2、XZ、ZST、7Z 等材料包。
- 智能体默认使用完全访问模式，直接读取用户选择的原项目，并可修改文件、执行 Shell/Bash；切换到 Plan 后只规划、不执行工具。
- 识别文档、源码、配置、图片、音视频、模型、归档和未知二进制资产。
- 从纯文本、OpenXML 和 PDF 内容流中提取可审计文本。
- 自动判断商业创新、软件、科研、社会实践和电商等竞赛类型。
- 生成 CPIR 项目画像，抽取成果声明并匹配证据文件。
- 检查跨文件名称、数据、时间、源码、构建入口和商业材料的一致性。
- 执行多赛道 JSON 规则，输出 blocker、warning、info 和可信债务。
- 生成可信评分、P0/P1/P2 补证任务、安全修订计划和二次审计差分。
- 通过会话工作区展示阶段进度、工具观察、项目上下文、历史任务和报告 artifact。
- 左侧工作台、中央会话区和右侧上下文面板协同展示历史会话、审计进度、材料预览、结果 artifact 与权限状态。
- 导出 Markdown/JSON 报告。
- 可选接入 DeepSeek 官方服务或 DeepSeek-compatible 自定义端点；智能体使用原生工具调用，最终评分始终由确定性规则产生。
- 支持浅色/深色外观、主题色、背景皮肤、字体和字号设置。

![完整项目自动审计结果](docs/images/workbench-audit.png)

## 快速开始

项目只使用系统工具链和系统开发包。CMake 预设会清空 Conda、`CMAKE_PREFIX_PATH` 和 `PKG_CONFIG_PATH` 对依赖解析的影响；任何解析到 Conda/Anaconda 目录的编译器或依赖都会使配置直接失败。

系统依赖包括（项目当前面向 Linux 系统工具链）：

- CMake 3.24+
- Ninja
- 支持 C++20 的 GCC 或 Clang
- Qt 6 Core、Gui、Qml、Quick、QuickControls2、QuickDialogs2
- OpenSSL、Zlib、libarchive、pugixml、nlohmann-json、Catch2
- clang-format、clang-tidy（完整质量检查需要）

Fedora 可使用系统包安装：

```bash
sudo dnf install cmake ninja-build gcc-c++ clang clang-tools-extra \
  qt6-qtbase-devel qt6-qtdeclarative-devel openssl-devel zlib-devel \
  libarchive-devel pugixml-devel json-devel catch-devel
```

构建、测试并启动：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
./build/debug/contest-workbench
```

Release 构建可使用对应预设：

```bash
cmake --preset release
cmake --build --preset release
```

也可以在启动时直接导入并审计项目：

```bash
./build/debug/contest-workbench --project /path/to/project
```

仓库内置完整演示材料：

```bash
./build/debug/contest-workbench \
  --project examples/full_competition_project_test_package
```

## 使用流程

1. 点击“添加项目”、拖入材料，或在 composer 中粘贴本地路径。
2. 智能体直接读取用户选择的原项目；需要评分时，审计流水线另行建立分析副本。
3. 在会话结论中先处理红色的“必须处理”，再处理黄色的“需要关注”。
4. 打开“完整检查结果”查看资产、画像、证据、一致性、问题和修改任务。
5. 如需直接修改原项目，可在完全访问模式下明确描述修改目标；如需先在安全副本中优化，执行 `/optimize`。
6. `/optimize` 会读回 `repaired-project` 的真实变更、执行二次审计并生成差分；确认结果后再决定如何应用到原项目。

> 注意：完全访问模式会直接作用于所选项目，适合可信项目和已纳入版本控制的工作目录。处理来源不明的材料或只想先看方案时，请先切换到 Plan。确定性审计在需要解包或分析时仍会建立有界工作副本。

常用命令：

| 命令 | 作用 |
|---|---|
| `/audit` | 重新运行确定性审计 |
| `/agent <任务>` | 提交智能体任务 |
| `/task <任务>` | `/agent` 的等价写法 |
| `/plan [目标]` | 切换到 Plan；带目标时先生成计划 |
| `/optimize [目标]` | 在 repaired-project 安全副本中修改并复审 |
| `/status` | 查看当前会话状态 |
| `/compact` | 压缩会话上下文 |
| `/clear` | 新建会话 |
| `/help` | 显示命令帮助 |

## 架构

```text
contest-workbench  Qt/QML 界面、会话交互、结果展示与导出入口
        │
contest_llm        可选 DeepSeek 请求、模型目录与原生工具调用循环
        │
contest_agent      工具注册、权限门控、Hooks、会话与分阶段编排
        │
contest_core       导入、识别、抽取、画像、证据、规则、评分、修复与报告
        │
JSON rule packs    多赛道可配置审计规则
```

核心业务逻辑不放在 QML 或 Controller 中。`contest_core` 不链接 DeepSeek/OpenSSL；网络能力独立位于 `contest_llm`。模型输出不能覆盖确定性评分。

## DeepSeek 配置（可选）

没有有效的 endpoint、模型和 API Key 组合时，全部确定性本地审计能力仍可使用，并且不会发起模型请求。智能语义修改（例如 `/optimize`）需要有效的 DeepSeek 配置。

```bash
export DEEPSEEK_API_KEY="..."
export DEEPSEEK_BASE_URL="https://api.deepseek.com"
export DEEPSEEK_MODEL="<可用 DeepSeek 模型 ID>"
```

`DEEPSEEK_AUTH_TOKEN` 可替代 `DEEPSEEK_API_KEY`，但两者不能同时配置。`DEEPSEEK_BASE_URL` 未设置时使用官方地址；模型 ID 不使用本地白名单，可在设置中按当前凭证读取模型目录。

也可以在应用目录或启动工作目录创建不会纳入 Git 的 `.env`：

```dotenv
LLM_PROVIDER=deepseek
DEEPSEEK_API_KEY=你的密钥
DEEPSEEK_BASE_URL=https://api.deepseek.com
DEEPSEEK_MODEL=deepseek-chat
```

系统环境变量优先于 `.env`。应用按“当前工作目录、可执行文件目录及其上两级目录”的顺序读取首个可用 `.env`，只接受 `LLM_PROVIDER`、`DEEPSEEK_API_KEY`、`DEEPSEEK_AUTH_TOKEN`、`DEEPSEEK_BASE_URL` 和 `DEEPSEEK_MODEL`，不会将密钥写入源码、报告或交付包。其他 provider 会被拒绝。

会话工作区默认位于系统应用数据目录下的 `workspaces/`；可在启动前通过 `CONTEST_WORKSPACE_ROOT` 覆盖：

```bash
export CONTEST_WORKSPACE_ROOT=/path/to/workspaces
```

## 质量保证

```bash
./tools/acceptance.sh
./tools/quality.sh
```

检查范围包括：

- GCC Debug 严格警告构建（`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`）；
- Clang ASan/UBSan 严格警告构建；
- 单元测试与固定依赖 smoke test；
- clang-format 和 clang-tidy；
- 全量 QML 静态检查与无界面启动检查；
- 系统依赖路径检查，禁止构建缓存和编译命令引用 Conda；
- 模块边界、归档安全、Workbench 结构和交付内容验收。

## 安全边界

- 完全访问模式下，智能体读取、搜索、Shell/Bash 和原项目写入直接作用于用户选择的路径；应配合版本控制或备份使用。
- 确定性审计仍可在 `<workspace-root>/<session_id>/input/` 建立分析副本，不再阻塞直接读取。
- 拒绝路径穿越、重复目标、文件/目录冲突、压缩炸弹和损坏归档。
- 超大、过深、加密、嵌套或预算外文件保留元数据并明确降级，不伪装成已审计。
- 不调用 shell `unzip`、`pdftotext` 或项目内脚本处理不可信输入。
- 不生成虚假用户、营收、合作、专利、实验结果或市场数据。
- Plan 模式阻断写入、命令执行、网络和 LLM 工具；切回完全访问后恢复相应能力。

## 文档与目录

```text
apps/contest-workbench/   Qt/QML 桌面端
include/cc/               公共 C++ 接口
src/                      Core、Agent、LLM 实现
rules/                    多赛道 JSON 规则包
tests/                    单元测试与依赖测试
examples/                 完整演示包和各赛道问题案例
tools/                    验收、质量检查和打包脚本
docs/                     需求、架构、规范、安全与验收文档
```

保留的详细文档：

- `docs/02_functional_requirements.md`：功能规格
- `docs/03_architecture_and_directory_tree.md`：架构与目录边界
- `docs/06_cpp_engineering_and_comment_style.md`：C++ 工程规范
- `docs/07_rule_pack_spec.md`：规则包规范
- `docs/08_security_model.md`：安全与权限模型
- `docs/09_test_and_acceptance.md`：测试和验收方案
- `docs/REQUIREMENT_AUDIT.md`：需求落实矩阵

## 打包

```bash
./tools/package_release.sh
```

命令会先执行 Debug 配置和构建，再通过 CPack 生成 `contest-project-trust-compiler-0.1.0-Linux.tar.gz`（平台后缀随系统变化），其中包含 Workbench、规则包、示例、工具、文档和 README。
