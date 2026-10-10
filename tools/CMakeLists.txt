add_executable(lsift_cli lsift_cli.cpp)
set_target_properties(lsift_cli PROPERTIES OUTPUT_NAME lsift)
target_link_libraries(lsift_cli PRIVATE LeanSift::lsift lsift_support)
if(WIN32)
  target_compile_definitions(lsift_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(lsift_bench bench.cpp)
target_link_libraries(lsift_bench PRIVATE LeanSift::lsift lsift_support)

# VLFeat is a DLL on Windows; put it next to the executables.
if(WIN32)
  foreach(t lsift_cli lsift_bench)
    add_custom_command(TARGET ${t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:vlfeat> $<TARGET_FILE_DIR:${t}>)
  endforeach()
endif()

# Kernel throughput (development aid). Uses internal symbols, so it needs the
# static library.
if(NOT BUILD_SHARED_LIBS)
  add_executable(lsift_microbench microbench.cpp)
  target_include_directories(lsift_microbench PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(lsift_microbench PRIVATE LeanSift::lsift hwy::hwy)
endif()
