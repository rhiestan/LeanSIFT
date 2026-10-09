# Dependencies used only by the tools, tests and benchmarks. The library itself
# depends on Google Highway alone.

# stb_image (public domain / MIT) for reading test images.
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20)
FetchContent_MakeAvailable(stb)
add_library(lsift_stb INTERFACE)
target_include_directories(lsift_stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

# VLFeat (BSD-2-Clause), the reference implementation LeanSift is compared against.
if(LSIFT_VLFEAT_SOURCE_DIR)
  set(vlfeat_SOURCE_DIR ${LSIFT_VLFEAT_SOURCE_DIR})
else()
  FetchContent_Declare(vlfeat
    GIT_REPOSITORY https://github.com/vlfeat/vlfeat.git
    GIT_TAG 1b9075fc42fe54b42f0e937f8b9a230d8e2c7701)
  FetchContent_GetProperties(vlfeat)
  if(NOT vlfeat_POPULATED)
    FetchContent_Populate(vlfeat)
  endif()
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
  target_link_libraries(vlfeat PRIVATE m)
endif()
if(NOT WIN32)
  target_link_libraries(vlfeat PRIVATE Threads::Threads)
endif()

set(LSIFT_VLFEAT_DATA_DIR ${vlfeat_SOURCE_DIR}/data CACHE INTERNAL "VLFeat sample images")
