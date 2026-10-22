# Dependencies used only by the tools, tests and benchmarks. The library itself
# depends on Google Highway alone.

# stb_image (public domain / MIT) for reading test images: stb/stb_image.h is an
# unmodified copy from https://github.com/nothings/stb at commit
# 2c980bb59875b0d32144a71867fbdebb2f77cd20.
add_library(lsift_stb INTERFACE)
target_include_directories(lsift_stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)

# VLFeat (BSD-2-Clause), the reference implementation LeanSift is compared against.
if(LSIFT_VLFEAT_SOURCE_DIR)
  set(vlfeat_SOURCE_DIR ${LSIFT_VLFEAT_SOURCE_DIR})
else()
  lsift_require_submodule(third_party/vlfeat vl/sift.c)
  set(vlfeat_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vlfeat)
endif()

enable_language(C)
set(VLFEAT_SOURCES
  ${vlfeat_SOURCE_DIR}/vl/generic.c
  ${vlfeat_SOURCE_DIR}/vl/host.c
  ${vlfeat_SOURCE_DIR}/vl/random.c
  ${vlfeat_SOURCE_DIR}/vl/imopv.c
  ${vlfeat_SOURCE_DIR}/vl/imopv_sse2.c
  ${vlfeat_SOURCE_DIR}/vl/mathop.c
  ${vlfeat_SOURCE_DIR}/vl/mathop_sse2.c
  ${vlfeat_SOURCE_DIR}/vl/sift.c)
if(WIN32)
  # VLFeat initialises its global state from DllMain on Windows, so it must be a DLL.
  add_library(vlfeat SHARED ${VLFEAT_SOURCES})
  target_compile_definitions(vlfeat PRIVATE VL_BUILD_DLL)
else()
  add_library(vlfeat STATIC ${VLFEAT_SOURCES})
  set_target_properties(vlfeat PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(vlfeat SYSTEM PUBLIC ${vlfeat_SOURCE_DIR})
target_compile_definitions(vlfeat PUBLIC VL_DISABLE_AVX VL_DISABLE_THREADS VL_DISABLE_OPENMP)
if(MSVC)
  target_compile_options(vlfeat PRIVATE /W0)
  # MSVC targets SSE2 on x64 without defining __SSE2__, which VLFeat checks.
  target_compile_definitions(vlfeat PRIVATE _CRT_SECURE_NO_WARNINGS $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:__SSE2__>)
else()
  target_compile_options(vlfeat PRIVATE -w)
endif()
if(NOT WIN32)
  target_link_libraries(vlfeat PRIVATE m Threads::Threads)
endif()

set(LSIFT_VLFEAT_DATA_DIR ${vlfeat_SOURCE_DIR}/data CACHE INTERNAL "VLFeat sample images")
