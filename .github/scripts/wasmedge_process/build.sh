#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The WasmEdge Authors

set -e

# Load WasmEdge environment if installed via install.sh
if [ -f "$HOME/.wasmedge/env" ]; then
  # shellcheck source=/dev/null
  source "$HOME/.wasmedge/env"
fi

BUILD_DIR="${BUILD_DIR:-build}"

cmake -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWASMEDGE_PLUGIN_PROCESS=ON \
  -DWASMEDGE_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --config Release -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"

ctest --test-dir "${BUILD_DIR}" -R wasmedgeProcessTests --output-on-failure
