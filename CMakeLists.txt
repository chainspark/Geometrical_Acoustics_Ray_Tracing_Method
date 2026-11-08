cmake_minimum_required(VERSION 3.20)
project(gart VERSION 2.0.0 LANGUAGES CXX
        DESCRIPTION "Geometrical acoustics ray tracing with phase (shoe-box room)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(EMSCRIPTEN)
  set(_wasm_default ON)
  set(_native_default OFF)
else()
  set(_wasm_default OFF)
  set(_native_default ON)
endif()

option(GART_BUILD_CLI   "Build the command-line simulator" ${_native_default})
option(GART_BUILD_TESTS "Build the unit and validation tests" ${_native_default})
option(GART_BUILD_WASM  "Build the WebAssembly module (requires Emscripten)" ${_wasm_default})

include(cmake/warnings.cmake)

# ---- core library ----------------------------------------------------------
add_library(gart_core STATIC core/src/analytical.cpp)
add_library(gart::core ALIAS gart_core)
target_include_directories(gart_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
target_compile_features(gart_core PUBLIC cxx_std_20)
gart_set_warnings(gart_core)

# ---- command-line app --------------------------------------------------------
if(GART_BUILD_CLI)
  find_package(Threads REQUIRED)
  add_executable(gart_cli apps/cli/main.cpp)
  set_target_properties(gart_cli PROPERTIES OUTPUT_NAME gart)
  target_link_libraries(gart_cli PRIVATE gart::core Threads::Threads)
  gart_set_warnings(gart_cli)
endif()

# ---- tests -----------------------------------------------------------------
if(GART_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ---- WebAssembly -------------------------------------------------------------
if(GART_BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "GART_BUILD_WASM requires the Emscripten toolchain (use emcmake cmake ...)")
  endif()
  add_subdirectory(wasm)
endif()
