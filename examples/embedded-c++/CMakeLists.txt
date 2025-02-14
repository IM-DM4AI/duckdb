cmake_minimum_required(VERSION 2.8.12...3.29)
project(example-c++)

set(PYTHON_INCLUDE_DIRS /root/miniconda3/envs/tpc_ai/include/python3.9)
set(PYTHON_LIBRARIES /root/miniconda3/envs/tpc_ai/lib/libpython3.9.so)
set(ARROW_PYTHON_INCLUDE_DIR /root/miniconda3/envs/tpc_ai/lib/python3.9/site-packages/pyarrow/include)
set(ARROW_PYTHON_LIB_DIR /root/miniconda3/envs/tpc_ai/lib/python3.9/site-packages/pyarrow)

set(CMAKE_CXX_STANDARD 17)

include_directories(../../src/include)
include_directories(${PYTHON_INCLUDE_DIRS} )
include_directories(${ARROW_PYTHON_INCLUDE_DIR})
link_directories(../../build/src)
link_directories(../../build/release/src)
link_directories(${ARROW_PYTHON_LIB_DIR})
link_directories(${PYTHON_LIBRARIES})

add_subdirectory(imbridge)
add_subdirectory(imbridge_test)

add_executable(example main.cpp)
target_link_libraries(example duckdb)

add_executable(udf udf.cpp)
target_link_libraries(udf duckdb)

add_executable(test_prediction_native test_prediction_native.cpp)
target_link_libraries(test_prediction_native duckdb)
