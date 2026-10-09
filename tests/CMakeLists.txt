set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE
  SYSTEM)
FetchContent_MakeAvailable(googletest)

add_executable(lsift_tests
  test_api.cpp
  test_executor.cpp
  test_matching.cpp
  test_targets.cpp
  test_vlfeat_agreement.cpp)
target_link_libraries(lsift_tests PRIVATE LeanSift::lsift lsift_support hwy::hwy GTest::gtest_main)
target_compile_definitions(lsift_tests PRIVATE LSIFT_TEST_DATA_DIR="${LSIFT_VLFEAT_DATA_DIR}")
if(WIN32)
  add_custom_command(TARGET lsift_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vlfeat> $<TARGET_FILE_DIR:lsift_tests>)
endif()

include(GoogleTest)
gtest_discover_tests(lsift_tests DISCOVERY_TIMEOUT 60)
