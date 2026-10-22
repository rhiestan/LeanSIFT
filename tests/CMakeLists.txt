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
  add_custom_command(TARGET lsift_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vlfeat> $<TARGET_FILE_DIR:lsift_tests>)
endif()

include(${PROJECT_SOURCE_DIR}/third_party/doctest/doctest.cmake)
doctest_discover_tests(lsift_tests)
