cmake_minimum_required(VERSION 2.8.12...3.29)
project(example-c++)

# set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include_directories(../../src/include)
# link_directories(../../build/release/src)
link_directories(../../build/src)

add_executable(example main.cpp)
target_link_libraries(example duckdb)

add_executable(test_prediction_native test_prediction_native.cpp)
target_link_libraries(test_prediction_native duckdb)

add_executable(trace_case trace_case.cpp)
target_link_libraries(trace_case duckdb)