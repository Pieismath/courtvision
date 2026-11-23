#!/bin/zsh
set -euo pipefail
CV_TESTS="$(cd "$(dirname "$0")" && pwd)"
CV_TEMP="$(mktemp -d)"
trap 'rm -rf "$CV_TEMP"' EXIT
clang++ -std=c++17 -O2 "$CV_TESTS/PhysicsTests.cpp" -o "$CV_TEMP/physics-tests"
"$CV_TEMP/physics-tests"
