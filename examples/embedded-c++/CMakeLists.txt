cmake_minimum_required(VERSION 2.8.12...3.29)
project(example-c++)

find_package(PythonLibs REQUIRED)
find_package(Arrow REQUIRED)

execute_process(
  COMMAND "python" -c "import sysconfig; print(sysconfig.get_path('include'))"
  OUTPUT_VARIABLE PYTHON_INCLUDE_DIRS_TMP
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT ${PYTHON_INCLUDE_DIRS} STREQUAL ${PYTHON_INCLUDE_DIRS_TMP})
  set(PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS_TMP})
  execute_process(
    COMMAND python -c "import sysconfig; import os; import glob; library_path = sysconfig.get_config_var('LIBDIR'); print(glob.glob(os.path.join(library_path, 'libpython*.so'))[0])"
    OUTPUT_VARIABLE PYTHON_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

execute_process(
  COMMAND "python" -c "import pyarrow; print(pyarrow.get_include())"
  OUTPUT_VARIABLE PYARROW_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND "python" -c "import pyarrow; print(pyarrow.get_library_dirs()[0])"
  OUTPUT_VARIABLE PYARROW_LIB_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


message(STATUS "PYTHON_INCLUDE_DIRS: ${PYTHON_INCLUDE_DIRS}")
message(STATUS "PYTHON_LIBRARIES: ${PYTHON_LIBRARIES}")
message(STATUS "PYARROW_INCLUDE_DIR: ${PYARROW_INCLUDE_DIR}")
message(STATUS "PYARROW_LIB_DIR: ${PYARROW_LIB_DIR}")

set(CMAKE_CXX_STANDARD 17)

include_directories(../../src/include)
include_directories(${PYTHON_INCLUDE_DIRS} )
include_directories(${PYARROW_INCLUDE_DIR})
link_directories(../../build/src)
link_directories(../../build/release/src)
link_directories(${PYARROW_LIB_DIR})
link_directories(${PYTHON_LIBRARIES})

add_subdirectory(imbridge)
add_subdirectory(imbridge_test)

add_executable(example main.cpp)
target_link_libraries(example duckdb)

add_executable(udf udf.cpp)
target_link_libraries(udf duckdb)

add_executable(test_prediction_native test_prediction_native.cpp)
target_link_libraries(test_prediction_native duckdb)
