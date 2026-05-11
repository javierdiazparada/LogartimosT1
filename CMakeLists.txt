cmake_minimum_required(VERSION 3.16)
project(static_rtree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rtree_core
  src/geometry.cpp
  src/disk.cpp
  src/bulkload.cpp
  src/query.cpp
  src/experiments.cpp
)
target_include_directories(rtree_core PUBLIC include)

function(rtree_target_options target_name)
  target_compile_options(${target_name} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )
endfunction()

rtree_target_options(rtree_core)

add_executable(rtree_cli src/main.cpp)
target_link_libraries(rtree_cli PRIVATE rtree_core)
rtree_target_options(rtree_cli)

enable_testing()
foreach(test_name test_io test_nearestx test_str test_query)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_link_libraries(${test_name} PRIVATE rtree_core)
  rtree_target_options(${test_name})
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
