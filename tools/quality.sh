#!/usr/bin/env bash
set -euo pipefail
shopt -s globstar nullglob

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PATH="/usr/local/bin:/usr/bin:/bin"
unset CONDA_PREFIX CONDA_DEFAULT_ENV CONDA_PROMPT_MODIFIER CMAKE_PREFIX_PATH PKG_CONFIG_PATH

cmake --preset debug
cmake --build --preset debug

clang-format --dry-run --Werror include/cc/**/*.hpp src/**/*.cpp \
  apps/contest-workbench/*.cpp apps/contest-workbench/*.hpp \
  tests/**/*.cpp tests/**/*.hpp tests/*.cpp tests/*.hpp

QMLLINT=""
if command -v qtpaths6 >/dev/null 2>&1; then
  QMLLINT="$(qtpaths6 --query QT_HOST_BINS)/qmllint"
elif command -v qmllint >/dev/null 2>&1; then
  QMLLINT="$(command -v qmllint)"
fi
if [[ -n "$QMLLINT" && -x "$QMLLINT" ]]; then
  QML_IMPORT_DIR="$(qtpaths6 --query QT_INSTALL_QML)"
  mapfile -t QML_FILES < <(find apps/contest-workbench/qml -type f -name '*.qml' -print | sort)
  "$QMLLINT" -I "$QML_IMPORT_DIR" -I apps/contest-workbench/qml \
    -I apps/contest-workbench/qml/components \
    -I apps/contest-workbench/qml/pages "${QML_FILES[@]}"
fi

QML_SMOKE_LOG="$(mktemp "${TMPDIR:-/tmp}/contest-qml-smoke.XXXXXX")"
set +e
QT_QPA_PLATFORM=offscreen timeout 5s build/debug/contest-workbench \
  >"$QML_SMOKE_LOG" 2>&1
QML_SMOKE_STATUS=$?
set -e
if [[ $QML_SMOKE_STATUS -ne 0 && $QML_SMOKE_STATUS -ne 124 ]]; then
  cat "$QML_SMOKE_LOG" >&2
  rm -f "$QML_SMOKE_LOG"
  echo "QML 启动检查失败" >&2
  exit 1
fi
if grep -E 'Failed to load|ReferenceError|TypeError|qrc:/qml/.*:[0-9]+:' \
  "$QML_SMOKE_LOG" >/dev/null; then
  cat "$QML_SMOKE_LOG" >&2
  rm -f "$QML_SMOKE_LOG"
  echo "QML 启动时出现运行错误" >&2
  exit 1
fi
rm -f "$QML_SMOKE_LOG"

clang-tidy -p build/debug \
  src/loader/LibArchiveReader.cpp \
  src/loader/PathGuard.cpp \
  src/loader/ZipArchiveReader.cpp \
  src/text/PdfContentStreamParser.cpp \
  src/llm/EndpointParser.cpp \
  src/llm/LlmBrain.cpp \
  src/inventory/InventoryEngine.cpp \
  src/rules/RuleConditionEvaluator.cpp \
  src/rules/RuleEngine.cpp \
  src/audit/TrustScoreCalculator.cpp \
  src/repair/RepairDiff.cpp \
  src/report/JsonReporter.cpp \
  --warnings-as-errors='bugprone-*,cppcoreguidelines-*' \
  --quiet

cmake --fresh --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure

if grep -E -l '/(mini)?conda[0-9]*/|/anaconda[0-9]*/|/mambaforge/' \
    build/asan/CMakeCache.txt build/asan/compile_commands.json >/dev/null; then
  echo "Sanitizer 构建不能引用 Conda/Anaconda 工具链或依赖" >&2
  exit 1
fi

echo "quality passed"
