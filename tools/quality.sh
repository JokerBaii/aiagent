#!/usr/bin/env bash
set -euo pipefail
shopt -s globstar nullglob

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake --preset debug
cmake --build --preset debug

clang-format --dry-run --Werror include/cc/**/*.hpp src/**/*.cpp \
  apps/contest-workbench/*.cpp apps/contest-workbench/*.hpp \
  tests/**/*.cpp tests/**/*.hpp tests/*.cpp tests/*.hpp

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

echo "quality passed"
