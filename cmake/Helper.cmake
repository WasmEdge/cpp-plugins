# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The WasmEdge Authors

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(FetchContent)

# Pinned WasmEdge release version for plugins
set(WASMEDGE_PINNED_VERSION "0.17.2" CACHE STRING "Target WasmEdge release version")

# Target setup helper
function(wasmedge_setup_target target)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    ENABLE_EXPORTS ON
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN ON
    BUILD_RPATH_USE_ORIGIN ON
    MACOSX_RPATH ON
  )
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "$ORIGIN"
    )
  elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "@loader_path"
    )
  endif()
endfunction()

function(wasmedge_add_library target)
  add_library(${target} ${ARGN})
  wasmedge_setup_target(${target})
endfunction()

function(wasmedge_add_executable target)
  add_executable(${target} ${ARGN})
  wasmedge_setup_target(${target})
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "$ORIGIN"
    )
  elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set_target_properties(${target} PROPERTIES
      INSTALL_RPATH "@executable_path"
    )
  endif()
endfunction()

# stb_image dependency helper for wasmedge_image
function(wasmedge_setup_stb_image)
  if(TARGET wasmedgeDepsSTBImage)
    return()
  endif()
  message(STATUS "Configuring stb_image dependency...")
  FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4 # stb_image v2.30
    GIT_SHALLOW    FALSE
  )
  FetchContent_MakeAvailable(stb)
  add_library(wasmedgeDepsSTBImage INTERFACE)
  target_include_directories(wasmedgeDepsSTBImage SYSTEM INTERFACE ${stb_SOURCE_DIR})
endfunction()

# Locate WasmEdge SDK and headers
macro(wasmedge_find_sdk)
  # 1. Locate WasmEdge shared library
  find_library(WASMEDGE_LIBRARY
    NAMES wasmedge
    HINTS
      ${WASMEDGE_LIB_DIR}
      $ENV{WASMEDGE_LIB_DIR}
      $ENV{WASMEDGE_DIR}/lib
      $ENV{HOME}/.wasmedge/lib
      /usr/local/lib
      /usr/lib
  )

  if(NOT WASMEDGE_LIBRARY)
    message(FATAL_ERROR "Could not find libwasmedge. Please install WasmEdge or set WASMEDGE_LIB_DIR.")
  endif()
  message(STATUS "Found WasmEdge shared library: ${WASMEDGE_LIBRARY}")

  # Define wasmedge_shared and wasmedgeCAPI imported targets
  if(NOT TARGET wasmedge_shared)
    add_library(wasmedge_shared UNKNOWN IMPORTED)
    set_target_properties(wasmedge_shared PROPERTIES
      IMPORTED_LOCATION "${WASMEDGE_LIBRARY}"
    )
  endif()
  if(NOT TARGET wasmedgeCAPI)
    add_library(wasmedgeCAPI UNKNOWN IMPORTED)
    set_target_properties(wasmedgeCAPI PROPERTIES
      IMPORTED_LOCATION "${WASMEDGE_LIBRARY}"
    )
  endif()

  # 2. Locate or fetch WasmEdge C++ headers
  set(WASMEDGE_HEADER_FOUND FALSE)
  if(WASMEDGE_INCLUDE_DIR)
    if(EXISTS "${WASMEDGE_INCLUDE_DIR}/plugin/plugin.h")
      set(WASMEDGE_HEADER_FOUND TRUE)
      set(WASMEDGE_CPP_INCLUDE_DIR "${WASMEDGE_INCLUDE_DIR}")
    endif()
  endif()

  if(NOT WASMEDGE_HEADER_FOUND)
    # Check common locations
    find_path(CANDIDATE_INCLUDE_DIR
      NAMES "plugin/plugin.h"
      HINTS
        $ENV{WASMEDGE_INCLUDE_DIR}
        $ENV{WASMEDGE_DIR}/include
        $ENV{HOME}/.wasmedge/include
        /usr/local/include
        /usr/include
    )
    if(CANDIDATE_INCLUDE_DIR)
      set(WASMEDGE_HEADER_FOUND TRUE)
      set(WASMEDGE_CPP_INCLUDE_DIR "${CANDIDATE_INCLUDE_DIR}")
    endif()
  endif()

  if(NOT WASMEDGE_HEADER_FOUND)
    message(STATUS "Fetching WasmEdge ${WASMEDGE_PINNED_VERSION} headers...")
    FetchContent_Declare(
      wasmedge_headers
      GIT_REPOSITORY https://github.com/WasmEdge/WasmEdge.git
      GIT_TAG        ${WASMEDGE_PINNED_VERSION}
      GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(wasmedge_headers)
    if(NOT wasmedge_headers_POPULATED)
      FetchContent_Populate(wasmedge_headers)
    endif()
    set(WASMEDGE_CPP_INCLUDE_DIR "${wasmedge_headers_SOURCE_DIR}/include")
  endif()
  message(STATUS "Using WasmEdge headers from: ${WASMEDGE_CPP_INCLUDE_DIR}")

  # 3. Locate WasmEdge C API headers
  find_path(WASMEDGE_C_INCLUDE_DIR
    NAMES "wasmedge/wasmedge.h" "wasmedge/enum_configure.h"
    HINTS
      ${WASMEDGE_INCLUDE_DIR}
      $ENV{WASMEDGE_INCLUDE_DIR}
      $ENV{WASMEDGE_DIR}/include
      $ENV{HOME}/.wasmedge/include
      /usr/local/include
      /usr/include
  )
  if(WASMEDGE_C_INCLUDE_DIR)
    message(STATUS "Using WasmEdge C API headers from: ${WASMEDGE_C_INCLUDE_DIR}")
  endif()

  # 4. Generate version.h and config.h in binary dir
  set(WASMEDGE_GEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen_include")
  file(MAKE_DIRECTORY "${WASMEDGE_GEN_INCLUDE_DIR}/common")
  if(NOT EXISTS "${WASMEDGE_GEN_INCLUDE_DIR}/common/version.h")
    file(WRITE "${WASMEDGE_GEN_INCLUDE_DIR}/common/version.h"
"// Generated by WasmEdge cpp-plugins standalone build
#pragma once
#include <string_view>
#include <cstdint>
namespace WasmEdge {
using namespace std::literals::string_view_literals;
static inline std::string_view kVersionString [[maybe_unused]] = \"${WASMEDGE_PINNED_VERSION}\"sv;
static inline std::string_view kGlobalPluginDir [[maybe_unused]] = \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/wasmedge\"sv;
static inline constexpr const uint32_t kPluginCurrentAPIVersion [[maybe_unused]] = 5;
} // namespace WasmEdge
"
    )
  endif()

  if(NOT EXISTS "${WASMEDGE_GEN_INCLUDE_DIR}/common/config.h")
    file(WRITE "${WASMEDGE_GEN_INCLUDE_DIR}/common/config.h"
"// Generated by WasmEdge cpp-plugins standalone build
#pragma once
#include <string_view>
namespace WasmEdge {
using namespace std::literals::string_view_literals;
#define HAVE_MMAP 1
#define HAVE_PWD_H 1
} // namespace WasmEdge
"
    )
  endif()

  # 5. Define wasmedgePlugin interface target with BOTH properties set
  if(NOT TARGET wasmedgePlugin)
    add_library(wasmedgePlugin INTERFACE)
    set(WASMEDGE_ALL_INCLUDES
      "${WASMEDGE_GEN_INCLUDE_DIR}"
      "${WASMEDGE_CPP_INCLUDE_DIR}"
      "${WASMEDGE_CPP_INCLUDE_DIR}/api"
    )
    if(WASMEDGE_C_INCLUDE_DIR)
      list(APPEND WASMEDGE_ALL_INCLUDES "${WASMEDGE_C_INCLUDE_DIR}")
    endif()
    set_target_properties(wasmedgePlugin PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${WASMEDGE_ALL_INCLUDES}"
      INCLUDE_DIRECTORIES "${WASMEDGE_ALL_INCLUDES}"
    )
  endif()
endmacro()
