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

# On Linux, vl/host.h pins memcpy to the symbol version GLIBC_2.2.5, which
# exists only on x86-64. Elsewhere, build from a copy of vl/ without that line.
set(vlfeat_code_dir ${vlfeat_SOURCE_DIR})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(vlfeat_code_dir ${CMAKE_CURRENT_BINARY_DIR}/vlfeat)
  file(COPY ${vlfeat_SOURCE_DIR}/vl DESTINATION ${vlfeat_code_dir} PATTERN host.h EXCLUDE)
  file(READ ${vlfeat_SOURCE_DIR}/vl/host.h _lsift_host_h)
  set(_lsift_symver [[__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");]])
  string(FIND "${_lsift_host_h}" "${_lsift_symver}" _lsift_found)
  if(_lsift_found EQUAL -1)
    message(FATAL_ERROR "The memcpy symbol version line was not found in vl/host.h")
  endif()
  string(REPLACE "${_lsift_symver}" "" _lsift_host_h "${_lsift_host_h}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/host.h.in "${_lsift_host_h}")
  configure_file(${CMAKE_CURRENT_BINARY_DIR}/host.h.in ${vlfeat_code_dir}/vl/host.h COPYONLY)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${vlfeat_SOURCE_DIR}/vl/host.h)
endif()

enable_language(C)
set(VLFEAT_SOURCES
  ${vlfeat_code_dir}/vl/generic.c
  ${vlfeat_code_dir}/vl/host.c
  ${vlfeat_code_dir}/vl/random.c
  ${vlfeat_code_dir}/vl/imopv.c
  ${vlfeat_code_dir}/vl/imopv_sse2.c
  ${vlfeat_code_dir}/vl/mathop.c
  ${vlfeat_code_dir}/vl/mathop_sse2.c
  ${vlfeat_code_dir}/vl/sift.c)
if(WIN32)
  # VLFeat initialises its global state from DllMain on Windows, so it must be a DLL.
  add_library(vlfeat SHARED ${VLFEAT_SOURCES})
  target_compile_definitions(vlfeat PRIVATE VL_BUILD_DLL)
else()
  add_library(vlfeat STATIC ${VLFEAT_SOURCES})
  set_target_properties(vlfeat PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(vlfeat SYSTEM PUBLIC ${vlfeat_code_dir})
target_compile_definitions(vlfeat PUBLIC VL_DISABLE_AVX VL_DISABLE_THREADS VL_DISABLE_OPENMP)
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86|x86)$")
  # VLFeat's SSE2 code is x86 only. Elsewhere it takes the byte order from
  # __LITTLE_ENDIAN__, which Clang defines but GCC does not.
  target_compile_definitions(vlfeat PUBLIC VL_DISABLE_SSE2)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    target_compile_definitions(vlfeat PUBLIC __LITTLE_ENDIAN__)
  endif()
endif()
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
