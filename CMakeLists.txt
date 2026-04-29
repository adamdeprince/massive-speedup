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

function(add_massive_speedup_module module_name module_source)
  nanobind_add_module(
    ${module_name}
    STABLE_ABI
    ${module_source}
    ${MASSIVE_SPEEDUP_COMMON_SOURCES}
  )

  configure_massive_speedup_target(${module_name})
  install(TARGETS ${module_name} LIBRARY DESTINATION massive_speedup)
endfunction()

add_massive_speedup_module(_native src/cpp/native.cpp)
