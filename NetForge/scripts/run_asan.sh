#!/usr/bin/env bash
# NetForge — run_asan.sh
# Builds with AddressSanitizer + UBSan and runs the test suite.

set -euo pipefail

BUILD_DIR="build-asan"

echo "Configuring with ASan + UBSan ..."
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DNETFORGE_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"

echo "Building ..."
cmake --build "${BUILD_DIR}" --parallel

echo "Running tests ..."
cd "${BUILD_DIR}"
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1" \
UBSAN_OPTIONS="print_stacktrace=1" \
ctest --output-on-failure

echo "All tests passed under ASan + UBSan."
