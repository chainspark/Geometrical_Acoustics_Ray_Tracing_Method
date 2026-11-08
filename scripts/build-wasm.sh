#!/usr/bin/env bash
# Builds the WebAssembly module into web/wasm/ (gart.js + gart.wasm).
# Uses a local emsdk if `emcmake` is on PATH, otherwise the emscripten/emsdk Docker image.
set -euo pipefail
cd "$(dirname "$0")/.."

EMSDK_IMAGE="${EMSDK_IMAGE:-emscripten/emsdk:3.1.74}"
BUILD_DIR="${BUILD_DIR:-build-wasm}"
CMDS="emcmake cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release && cmake --build $BUILD_DIR -j"

if [[ "${USE_DOCKER:-auto}" != "1" ]] && command -v emcmake >/dev/null; then
  bash -c "$CMDS"
else
  SRC="$(pwd -W 2>/dev/null || pwd)"
  MSYS_NO_PATHCONV=1 docker run --rm -v "$SRC":/src -w /src "$EMSDK_IMAGE" bash -c "$CMDS"
fi
ls -l web/wasm
