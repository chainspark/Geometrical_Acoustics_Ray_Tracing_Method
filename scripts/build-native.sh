#!/usr/bin/env bash
# Builds the native CLI and runs the tests.
# Uses the host toolchain if cmake + a C++20 compiler are available, otherwise
# runs inside the emscripten/emsdk Docker image (which ships cmake and g++).
set -euo pipefail
cd "$(dirname "$0")/.."

EMSDK_IMAGE="${EMSDK_IMAGE:-emscripten/emsdk:3.1.74}"
BUILD_DIR="${BUILD_DIR:-build-native}"
CMDS="cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release && \
      cmake --build $BUILD_DIR -j && \
      ctest --test-dir $BUILD_DIR --output-on-failure ${CTEST_ARGS:-}"

if [[ "${USE_DOCKER:-auto}" != "1" ]] && command -v cmake >/dev/null && { command -v c++ >/dev/null || command -v cl >/dev/null; }; then
  bash -c "$CMDS"
else
  SRC="$(pwd -W 2>/dev/null || pwd)"
  MSYS_NO_PATHCONV=1 docker run --rm -v "$SRC":/src -w /src "$EMSDK_IMAGE" bash -c "$CMDS"
fi
