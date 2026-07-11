#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake --preset debug
cmake --build --preset debug
cpack --config build/debug/CPackConfig.cmake

echo "package files:"
find . build/debug -maxdepth 1 -type f -name '*.tar.gz' -print
