cmake_minimum_required(VERSION 3.21)

project(LeanSift
  VERSION 0.1.0
  DESCRIPTION "Fast SIMD and multi-threaded SIFT, compatible with VLFeat"
  LANGUAGES CXX)

option(LSIFT_BUILD_TESTS "Build the LeanSift tests" ${PROJECT_IS_TOP_LEVEL})
option(LSIFT_BUILD_TOOLS "Build the lsift command-line tool and benchmark" ${PROJECT_IS_TOP_LEVEL})
option(LSIFT_USE_SYSTEM_HWY "Use an installed Google Highway (find_package) instead of the submodule" OFF)
option(LSIFT_INSTALL "Generate the install target" ${PROJECT_IS_TOP_LEVEL})
set(LSIFT_VLFEAT_SOURCE_DIR "" CACHE PATH
  "VLFeat source tree used as the reference in tests and benchmarks (the submodule when empty)")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# Stops with a helpful message when the submodule in `dir` has not been checked
# out (`file` is a file it must contain).
function(lsift_require_submodule dir file)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${dir}/${file}")
    message(FATAL_ERROR "${dir} is missing. Run\n"
      "  git submodule update --init --recursive\n"
      "or clone with --recursive.")
  endif()
endfunction()

# Treats the include directories of `target` as system headers, so that our
# warning flags (-Wpedantic) do not apply to them.
function(lsift_mark_system target)
  get_target_property(_includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
  if(_includes)
    set_target_properties(${target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_includes}")
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Google Highway (portable SIMD with runtime dispatch)
# ---------------------------------------------------------------------------
if(LSIFT_USE_SYSTEM_HWY)
  find_package(hwy 1.0 REQUIRED)
else()
  lsift_require_submodule(third_party/highway CMakeLists.txt)
  set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
  set(HWY_ENABLE_INSTALL ${LSIFT_INSTALL} CACHE BOOL "" FORCE)
  set(HWY_FORCE_STATIC_LIBS ON CACHE BOOL "" FORCE)
  add_subdirectory(third_party/highway)
  lsift_mark_system(hwy)
  if(NOT TARGET hwy::hwy)
    add_library(hwy::hwy ALIAS hwy)
  endif()
endif()

add_subdirectory(src)

if(LSIFT_BUILD_TESTS OR LSIFT_BUILD_TOOLS)
  add_subdirectory(third_party)
  add_subdirectory(support)
endif()

if(LSIFT_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(LSIFT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
