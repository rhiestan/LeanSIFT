# Helpers shared by the tests, tools and benchmarks: image loading, a runner
# for the VLFeat reference implementation and the comparison metrics.
add_library(lsift_support STATIC
  compare.cpp
  image_io.cpp
  vlfeat_ref.cpp)
target_include_directories(lsift_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lsift_support PUBLIC LeanSift::lsift PRIVATE lsift_stb vlfeat)
target_compile_features(lsift_support PUBLIC cxx_std_17)
if(WIN32)
  target_compile_definitions(lsift_support PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
