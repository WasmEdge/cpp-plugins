# WasmEdge C++ Plugins

This repository hosts standalone C++ plugins for [WasmEdge](https://github.com/WasmEdge/WasmEdge).

Plugins are built standalone against a released WasmEdge SDK rather than tracking unreleased WasmEdge `master`.

## Plugins

| Plugin | Build Option | Platforms | Status |
| :--- | :--- | :--- | :--- |
| `wasmedge_image` | `-DWASMEDGE_PLUGIN_IMAGE=ON` | Linux, macOS | Migrated |
| `wasmedge_process` | `-DWASMEDGE_PLUGIN_PROCESS=ON` | Linux | Migrated |

## Prerequisites

1. **CMake** 3.16 or later (3.24+ recommended)
2. **C++17 Compiler** (GCC, Clang, or MSVC)
3. **WasmEdge SDK** (version 0.17.2 or later)

To install WasmEdge:
```bash
curl -sSf https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash -s -- -v 0.17.2
```

## Building Plugins

Plugins are configured with all `WASMEDGE_PLUGIN_*` options set to `OFF` by default. Enable only the plugins you wish to build:

```bash
# Build wasmedge_process
cmake -B build -DWASMEDGE_PLUGIN_PROCESS=ON
cmake --build build

# Build wasmedge_image
cmake -B build -DWASMEDGE_PLUGIN_IMAGE=ON
cmake --build build
```

The compiled plugin shared libraries will be placed under `build/plugins/<plugin_name>/`.

### Locating the WasmEdge SDK

The build system searches for the WasmEdge library and headers in common installation directories (`$HOME/.wasmedge`, `/usr/local`, `/usr`).

You can explicitly provide custom paths using CMake variables or environment variables:
- `WASMEDGE_DIR`: Path to the WasmEdge installation prefix (e.g. `~/.wasmedge`)
- `WASMEDGE_LIB_DIR`: Path to directory containing `libwasmedge.so` / `libwasmedge.dylib`
- `WASMEDGE_INCLUDE_DIR`: Path to WasmEdge headers directory

If headers are not found locally, CMake will automatically fetch headers for the pinned WasmEdge release.

## Running Tests

To build and execute plugin unit tests:

```bash
cmake -B build \
  -DWASMEDGE_PLUGIN_PROCESS=ON \
  -DWASMEDGE_PLUGIN_IMAGE=ON \
  -DWASMEDGE_BUILD_TESTS=ON

cmake --build build
ctest --test-dir build --output-on-failure
```

## Upgrade Policy

Plugins in this repository sync to new WasmEdge APIs **only when WasmEdge cuts an official release**. The minimum target release is pinned via `WASMEDGE_PINNED_VERSION` in `cmake/Helper.cmake` (currently `0.17.2`). Maintainers should not track or depend on unreleased `master` branch changes.
