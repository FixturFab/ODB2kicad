#!/bin/bash
# Build and run the odb2kicad unit tests with plain g++ (no cmake needed).
#   test/run_unit_tests.sh [name-filter]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${HERE}/../src"
OUT="${BUILD_DIR:-${HERE}/../build}"
mkdir -p "${OUT}"
g++ -std=c++17 -O1 -I"${SRC}" "${HERE}/unit/unit_tests.cpp" \
    $(find "${SRC}" -name '*.cpp' ! -name main.cpp ! -name wasm_bindings.cpp) -o "${OUT}/odb2kicad_unit_tests"
"${OUT}/odb2kicad_unit_tests" "$@"
