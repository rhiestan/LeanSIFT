set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
lsift_require_submodule(third_party/googletest CMakeLists.txt)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/googletest ${CMAKE_CURRENT_BINARY_DIR}/googletest)
lsift_mark_system(gtest)
lsift_mark_system(gtest_main)

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
