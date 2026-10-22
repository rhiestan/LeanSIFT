add_executable(lsift_tests
  main.cpp
  test_api.cpp
  test_executor.cpp
  test_matching.cpp
  test_targets.cpp
  test_vlfeat_agreement.cpp)
target_link_libraries(lsift_tests PRIVATE LeanSift::lsift lsift_support hwy::hwy)
# doctest (MIT): doctest.h, doctest.cmake and doctestAddTests.cmake are unmodified
# copies from https://github.com/doctest/doctest, release v2.5.3.
target_include_directories(lsift_tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/doctest)
target_compile_definitions(lsift_tests PRIVATE LSIFT_TEST_DATA_DIR="${LSIFT_VLFEAT_DATA_DIR}")
if(WIN32)
  target_compile_definitions(lsift_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  add_custom_command(TARGET lsift_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vlfeat> $<TARGET_FILE_DIR:lsift_tests>)
endif()

include(${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake)
doctest_discover_tests(lsift_tests)

# ---------------------------------------------------------------------------
# Emulated CPUs. Targets.EveryTargetAgreesWithVLFeat checks every SIMD target
# that the CPU supports; these tests run it under an emulator to also cover the
# targets this CPU lacks, and require that the emulated CPU ran all of them.
set(LSIFT_SDE "" CACHE FILEPATH
  "Intel SDE executable (sde64 or sde.exe), to also test the AVX-512 targets")
set(LSIFT_SDE_CPU "dmr" CACHE STRING
  "CPU emulated by SDE: a directory in SDE's misc/cpuid (dmr: Diamond Rapids)")
set(LSIFT_QEMU "" CACHE FILEPATH
  "qemu-aarch64 executable, to also test the SVE targets with several vector lengths")

if(LSIFT_SDE)
  # Highway uses AVX3_ZEN4 only on AMD CPUs, so the emulated CPU reports AMD
  # as its vendor (CPUID leaf 0); its features are those of LSIFT_SDE_CPU.
  get_filename_component(_sde_dir "${LSIFT_SDE}" DIRECTORY)
  set(_cpuid_def "${_sde_dir}/misc/cpuid/${LSIFT_SDE_CPU}/cpuid.def")
  if(NOT EXISTS "${_cpuid_def}")
    message(FATAL_ERROR "LSIFT_SDE_CPU: ${_cpuid_def} does not exist")
  endif()
  file(READ "${_cpuid_def}" _cpuid)
  # "Genu" "ntel" "ineI" (EBX ECX EDX) -> "Auth" "cAMD" "enti"
  string(REGEX REPLACE "(\n00000000 [*]+ => [0-9a-f]+) 756e6547 6c65746e 49656e69"
    "\\1 68747541 444d4163 69746e65" _cpuid_amd "${_cpuid}")
  if(_cpuid_amd STREQUAL _cpuid)
    message(FATAL_ERROR "No Intel vendor string found in ${_cpuid_def}")
  endif()
  set(_cpuid_amd_def "${CMAKE_CURRENT_BINARY_DIR}/sde-${LSIFT_SDE_CPU}-amd.def")
  file(WRITE "${_cpuid_amd_def}" "${_cpuid_amd}")
  add_test(NAME Targets.SDE
    COMMAND "${LSIFT_SDE}" -cpuid_in "${_cpuid_amd_def}" --
            $<TARGET_FILE:lsift_tests> --test-case=Targets.EveryTargetAgreesWithVLFeat)
  set_tests_properties(Targets.SDE PROPERTIES
    ENVIRONMENT LSIFT_REQUIRE_TARGETS=all LABELS emulated TIMEOUT 3600)
endif()

if(LSIFT_QEMU)
  # The vector length is in bytes. Highway has targets for 128-bit SVE2
  # (SVE2_128) and 256-bit SVE (SVE_256); other lengths use SVE and SVE2.
  foreach(_bytes 16 32 64)
    math(EXPR _bits "${_bytes} * 8")
    if(_bytes EQUAL 16)
      set(_required SVE,SVE2,SVE2_128)
    elseif(_bytes EQUAL 32)
      set(_required SVE,SVE2,SVE_256)
    else()
      set(_required SVE,SVE2)
    endif()
    add_test(NAME Targets.QEMU_SVE${_bits}
      COMMAND "${LSIFT_QEMU}" -cpu max,sve-default-vector-length=${_bytes}
              $<TARGET_FILE:lsift_tests> --test-case=Targets.EveryTargetAgreesWithVLFeat)
    set_tests_properties(Targets.QEMU_SVE${_bits} PROPERTIES
      ENVIRONMENT LSIFT_REQUIRE_TARGETS=${_required} LABELS emulated TIMEOUT 3600)
  endforeach()
endif()
