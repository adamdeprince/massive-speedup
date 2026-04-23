cmake_minimum_required(VERSION 3.24)
project(massive_speedup LANGUAGES CXX)

include(FetchContent)

find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG REQUIRED)

set(
  FETCHCONTENT_UPDATES_DISCONNECTED
  ON
  CACHE BOOL
  "Do not contact dependency remotes for updates after FetchContent sources exist."
)

option(MASSIVE_SPEEDUP_USE_SYSTEM_SIMDJSON "Use an already-installed simdjson package." OFF)
option(MASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP "Use a pre-vendored rapidgzip checkout." OFF)
option(
  MASSIVE_SPEEDUP_OFFLINE
  "Never download missing third-party dependency sources."
  OFF
)
set(
  MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR
  ""
  CACHE PATH
  "Path to a local rapidgzip source checkout when MASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP=ON."
)

set(MASSIVE_SPEEDUP_HAS_SIMDJSON_VALUE 1)
set(MASSIVE_SPEEDUP_HAS_RAPIDGZIP_VALUE 1)
set(MASSIVE_SPEEDUP_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps")

if(MASSIVE_SPEEDUP_USE_SYSTEM_SIMDJSON)
  find_package(simdjson REQUIRED)
elseif(EXISTS "${MASSIVE_SPEEDUP_DEPS_DIR}/simdjson-src/CMakeLists.txt")
  set(SIMDJSON_JUST_LIBRARY ON CACHE INTERNAL "")
  set(SIMDJSON_BUILD_STATIC ON CACHE INTERNAL "")
  FetchContent_Declare(
    simdjson
    SOURCE_DIR "${MASSIVE_SPEEDUP_DEPS_DIR}/simdjson-src"
    BINARY_DIR "${MASSIVE_SPEEDUP_DEPS_DIR}/simdjson-build"
  )
  FetchContent_MakeAvailable(simdjson)
elseif(MASSIVE_SPEEDUP_OFFLINE)
  message(
    WARNING
    "simdjson source checkout is missing at ${MASSIVE_SPEEDUP_DEPS_DIR}/simdjson-src; "
    "building without C++ simdjson integration because MASSIVE_SPEEDUP_OFFLINE=ON."
  )
  add_library(simdjson INTERFACE)
  set(MASSIVE_SPEEDUP_HAS_SIMDJSON_VALUE 0)
else()
  set(SIMDJSON_JUST_LIBRARY ON CACHE INTERNAL "")
  set(SIMDJSON_BUILD_STATIC ON CACHE INTERNAL "")
  FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.13.0
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
  )
  FetchContent_MakeAvailable(simdjson)
endif()

if(MASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP)
  if(NOT MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR)
    message(
      FATAL_ERROR
      "Set MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR when MASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP=ON."
    )
  endif()

  add_library(rapidgzip INTERFACE)
  target_include_directories(
    rapidgzip
    INTERFACE
    ${MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR}
    ${MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR}/src
    ${MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR}/librapidarchive/src
  )
elseif(
  EXISTS "${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src/src/rapidgzip/ParallelGzipReader.hpp"
  OR EXISTS "${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src/librapidarchive/src/rapidgzip/ParallelGzipReader.hpp"
)
  add_library(rapidgzip INTERFACE)
  target_include_directories(
    rapidgzip
    INTERFACE
    ${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src
    ${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src/src
    ${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src/librapidarchive/src
  )
elseif(MASSIVE_SPEEDUP_OFFLINE)
  message(
    FATAL_ERROR
    "rapidgzip source checkout is missing at ${MASSIVE_SPEEDUP_DEPS_DIR}/rapidgzip-src. "
    "Disable MASSIVE_SPEEDUP_OFFLINE to download it or set "
    "MASSIVE_SPEEDUP_USE_SYSTEM_RAPIDGZIP=ON with MASSIVE_SPEEDUP_RAPIDGZIP_SOURCE_DIR."
  )
else()
  FetchContent_Declare(
    rapidgzip
    GIT_REPOSITORY https://github.com/mxmlnkn/rapidgzip.git
    GIT_TAG rapidgzip-v0.16.0
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
  )
  FetchContent_GetProperties(rapidgzip)
  if(NOT rapidgzip_POPULATED)
    FetchContent_Populate(rapidgzip)
  endif()

  add_library(rapidgzip INTERFACE)
  target_include_directories(
    rapidgzip
    INTERFACE
    ${rapidgzip_SOURCE_DIR}
    ${rapidgzip_SOURCE_DIR}/src
    ${rapidgzip_SOURCE_DIR}/librapidarchive/src
  )
endif()

add_library(massive_speedup_cppdeps INTERFACE)
target_link_libraries(
  massive_speedup_cppdeps
  INTERFACE
  rapidgzip
  simdjson
)
target_compile_definitions(
  massive_speedup_cppdeps
  INTERFACE
  MASSIVE_SPEEDUP_HAS_RAPIDGZIP=${MASSIVE_SPEEDUP_HAS_RAPIDGZIP_VALUE}
  MASSIVE_SPEEDUP_HAS_SIMDJSON=${MASSIVE_SPEEDUP_HAS_SIMDJSON_VALUE}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  string(REPLACE "-O0" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O3" CACHE STRING "" FORCE)
elseif(MSVC)
  string(REPLACE "/Od" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /O2" CACHE STRING "" FORCE)
endif()

set(MASSIVE_SPEEDUP_COMMON_SOURCES
  src/cpp/parser_common.cpp
  src/cpp/flatfiles.cpp
  src/cpp/websocket.cpp
)

function(configure_massive_speedup_target target_name)
  target_compile_features(${target_name} PRIVATE cxx_std_23)
  target_include_directories(${target_name} PRIVATE include src/cpp)
  target_link_libraries(${target_name} PRIVATE massive_speedup_cppdeps)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -O3 -Wall -Wextra -Wpedantic)
  endif()

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /O2 /W4)
  endif()
endfunction()

function(add_massive_speedup_backend module_name backend_source)
  nanobind_add_module(
    ${module_name}
    STABLE_ABI
    ${backend_source}
    src/cpp/cpu.cpp
    ${MASSIVE_SPEEDUP_COMMON_SOURCES}
  )

  configure_massive_speedup_target(${module_name})
  install(TARGETS ${module_name} LIBRARY DESTINATION massive_speedup)
endfunction()

nanobind_add_module(
  _cpu
  STABLE_ABI
  src/cpp/cpu_module.cpp
  src/cpp/cpu.cpp
)

configure_massive_speedup_target(_cpu)

set(MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS)
list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_GENERIC=1)

add_massive_speedup_backend(_generic src/cpp/backends/generic.cpp)

install(TARGETS _cpu LIBRARY DESTINATION massive_speedup)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
  add_massive_speedup_backend(_sse src/cpp/backends/x86_sse.cpp)
  target_compile_definitions(_sse PRIVATE MASSIVE_SPEEDUP_PROCESSOR_SSE42=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_sse PRIVATE -msse4.2)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_X86_SSE42=1)

  add_massive_speedup_backend(_avx src/cpp/backends/x86_avx.cpp)
  target_compile_definitions(_avx PRIVATE MASSIVE_SPEEDUP_PROCESSOR_AVX=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_avx PRIVATE -mavx2)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_X86_AVX2=1)

  add_massive_speedup_backend(_avx512 src/cpp/backends/x86_avx512.cpp)
  target_compile_definitions(_avx512 PRIVATE MASSIVE_SPEEDUP_PROCESSOR_AVX512BW=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_avx512 PRIVATE -mavx512f -mavx512bw)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_X86_AVX512=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  add_massive_speedup_backend(_neon src/cpp/backends/linux_aarch64_neon.cpp)
  target_compile_definitions(_neon PRIVATE MASSIVE_SPEEDUP_PROCESSOR_NEON=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_neon PRIVATE -march=armv8-a+simd)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_NEON=1)

  add_massive_speedup_backend(_sve src/cpp/backends/linux_aarch64_sve.cpp)
  target_compile_definitions(_sve PRIVATE MASSIVE_SPEEDUP_PROCESSOR_SVE=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_sve PRIVATE -march=armv8.2-a+sve)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_SVE=1)

  add_massive_speedup_backend(_sve2 src/cpp/backends/linux_aarch64_sve2.cpp)
  target_compile_definitions(_sve2 PRIVATE MASSIVE_SPEEDUP_PROCESSOR_SVE2=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_sve2 PRIVATE -march=armv9-a+sve2)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_SVE2=1)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(loongarch64|loong64)$")
  add_massive_speedup_backend(_lsx src/cpp/backends/linux_loongarch64_lsx.cpp)
  target_compile_definitions(_lsx PRIVATE MASSIVE_SPEEDUP_PROCESSOR_LSX=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_lsx PRIVATE -mlsx)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_LINUX_LOONGARCH64_LSX=1)

  add_massive_speedup_backend(_lasx src/cpp/backends/linux_loongarch64_lasx.cpp)
  target_compile_definitions(_lasx PRIVATE MASSIVE_SPEEDUP_PROCESSOR_LASX=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(_lasx PRIVATE -mlasx)
  endif()
  list(APPEND MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS MASSIVE_SPEEDUP_HAVE_LINUX_LOONGARCH64_LASX=1)
endif()

target_compile_definitions(_cpu PRIVATE ${MASSIVE_SPEEDUP_CPU_COMPILE_DEFINITIONS})
