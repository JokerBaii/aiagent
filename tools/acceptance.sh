#!/usr/bin/env bash
set -euo pipefail
shopt -s globstar nullglob

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
TEST_WORKSPACE="$(mktemp -d "${TMPDIR:-/tmp}/contest-acceptance.XXXXXX")"
export CONTEST_WORKSPACE_ROOT="$TEST_WORKSPACE"
trap 'rm -rf "$TEST_WORKSPACE"' EXIT

cmake --fresh --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

grep -q 'find_package(nlohmann_json CONFIG REQUIRED)' CMakeLists.txt
grep -q 'find_package(Catch2 CONFIG REQUIRED)' CMakeLists.txt
grep -q 'find_package(OpenSSL REQUIRED)' CMakeLists.txt
grep -q 'LIBARCHIVE_INCLUDE_DIR' CMakeLists.txt
grep -q 'LibArchive::LibArchive' CMakeLists.txt
grep -q 'PUGIXML_LIBRARY' CMakeLists.txt
grep -q 'Contest::Pugixml' CMakeLists.txt
grep -q 'nlohmann/json.hpp' src/core/JsonValue.cpp
grep -q 'pugixml.hpp' src/text/OpenXmlTextExtractor.cpp
grep -q 'catch2/catch_test_macros.hpp' tests/catch2/DependencySmokeTests.cpp
grep -q 'contest_dependency_tests' CMakeLists.txt
grep -q 'ZipArchiveReader' src/loader/ArchiveExtractor.cpp
grep -q 'LibArchiveReader' src/loader/ArchiveExtractor.cpp

if grep -E 'popen|std::system|unzip|pdftotext' \
    src/loader/ArchiveExtractor.cpp \
    src/loader/ZipArchiveReader.cpp \
    src/loader/LibArchiveReader.cpp \
    src/text/OpenXmlTextExtractor.cpp \
    src/text/PdfTextExtractor.cpp \
    src/text/PdfContentStreamParser.cpp >/dev/null; then
  echo "zip/OpenXML/PDF 文本抽取不能依赖 shell 或外部工具" >&2
  exit 1
fi
if ar t build/debug/libcontest_core.a | grep -E 'Llm|HttpsJsonClient|EndpointParser' >/dev/null; then
  echo "contest_core 不能包含 LLM/OpenSSL 客户端对象" >&2
  exit 1
fi
if ar t build/debug/libcontest_core.a | grep -E 'PermissionGate|LifecycleHookManager|ToolRegistry|AuditSessionStore|AgentRuntime|StagedAuditPipeline' >/dev/null; then
  echo "contest_core 不能包含 agentic runtime 对象" >&2
  exit 1
fi
if ! ar t build/debug/libcontest_agent.a | grep -E 'PermissionGate|LifecycleHookManager|ToolRegistry|AuditSessionStore|AgentRuntime|StagedAuditPipeline' >/dev/null; then
  echo "contest_agent 没有包含 agentic runtime 对象" >&2
  exit 1
fi
if ! ar t build/debug/libcontest_llm.a | grep -E 'LlmBrain|HttpsJsonClient' >/dev/null; then
  echo "contest_llm 没有包含 LLM Brain 对象" >&2
  exit 1
fi
if [ -e build/debug/contest-compiler ]; then
  echo "旧的文本入口产物不应继续存在" >&2
  exit 1
fi

clang-format --dry-run --Werror include/cc/**/*.hpp src/**/*.cpp \
  apps/contest-workbench/*.cpp apps/contest-workbench/*.hpp \
  tests/**/*.cpp tests/**/*.hpp tests/*.cpp tests/*.hpp

grep -q 'SessionWorkspacePage' apps/contest-workbench/resources.qrc
grep -q 'SessionWorkspacePage' apps/contest-workbench/qml/Main.qml
grep -q 'sessionHistory' apps/contest-workbench/CompileController.hpp
grep -q 'toolCards' apps/contest-workbench/CompileController.hpp
grep -q 'permissionCards' apps/contest-workbench/CompileController.hpp
grep -q 'artifacts' apps/contest-workbench/CompileController.hpp
grep -q 'submitMessage' apps/contest-workbench/CompileController.hpp
grep -q 'compiler.sessionHistory' apps/contest-workbench/qml/pages/SessionWorkspacePage.qml
grep -q 'dropActive' apps/contest-workbench/qml/pages/SessionWorkspacePage.qml
grep -q 'compiler.projectContext' apps/contest-workbench/qml/Main.qml
grep -q 'compiler.permissionCards' apps/contest-workbench/qml/Main.qml
grep -q 'compiler.artifacts' apps/contest-workbench/qml/Main.qml
if grep -q 'rightPanelTab === "skills"' apps/contest-workbench/qml/Main.qml; then
  echo "技能面板应隐式化，不能作为右侧常驻面板" >&2
  exit 1
fi
if grep -R 'ProjectImportPage' apps/contest-workbench/qml apps/contest-workbench/resources.qrc >/dev/null; then
  echo "旧 ProjectImportPage 已被会话工作区覆盖，不能继续注册或引用" >&2
  exit 1
fi

echo "acceptance passed"
