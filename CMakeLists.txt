# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The WasmEdge Authors

cmake_minimum_required(VERSION 3.16)
project(wasmedge_plugins C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Helper)

# Plugin build options (default: OFF, per Issue #24)
option(WASMEDGE_PLUGIN_PROCESS "Build wasmedge_process plugin" OFF)
option(WASMEDGE_PLUGIN_IMAGE "Build wasmedge_image plugin" OFF)
option(WASMEDGE_PLUGIN_ZLIB "Build wasmedge_zlib plugin" OFF)
option(WASMEDGE_PLUGIN_WASI_CRYPTO "Build wasi_crypto plugin" OFF)
option(WASMEDGE_PLUGIN_WASI_HTTP "Build wasi_http plugin" OFF)
option(WASMEDGE_PLUGIN_WASI_NN_BACKEND "Build wasi_nn plugin" OFF)
option(WASMEDGE_PLUGIN_WASI_POLL "Build wasi_poll plugin" OFF)
option(WASMEDGE_PLUGIN_WASM_BPF "Build wasm_bpf plugin" OFF)
option(WASMEDGE_PLUGIN_FFMPEG "Build wasmedge_ffmpeg plugin" OFF)
option(WASMEDGE_PLUGIN_OCR "Build wasmedge_ocr plugin" OFF)
option(WASMEDGE_PLUGIN_OPENCVMINI "Build wasmedge_opencvmini plugin" OFF)
option(WASMEDGE_PLUGIN_STABLEDIFFUSION "Build wasmedge_stablediffusion plugin" OFF)
option(WASMEDGE_PLUGIN_TENSORFLOW "Build wasmedge_tensorflow plugin" OFF)
option(WASMEDGE_PLUGIN_TENSORFLOWLITE "Build wasmedge_tensorflowlite plugin" OFF)
option(WASMEDGE_PLUGIN_UNITTESTS "Build plugin loader unit tests" OFF)

option(WASMEDGE_BUILD_TESTS "Build plugin tests" OFF)
option(WASMEDGE_LINK_PLUGINS_STATIC "Link plugins statically against WasmEdge" OFF)

# Locate WasmEdge SDK and headers
wasmedge_find_sdk()

# Build enabled plugins
add_subdirectory(plugins)

# Build tests if requested
if(WASMEDGE_BUILD_TESTS)
  enable_testing()
  add_compile_definitions(FMT_HEADER_ONLY=1)
  find_package(GTest QUIET)
  if(NOT TARGET GTest::gtest)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googletest)
  endif()
  set(GTEST_BOTH_LIBRARIES GTest::gtest GTest::gtest_main)
  add_subdirectory(test/plugins)
endif()
