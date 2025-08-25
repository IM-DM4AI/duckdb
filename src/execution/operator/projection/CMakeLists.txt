add_library_unity(
  duckdb_operator_projection OBJECT physical_projection.cpp
  physical_tableinout_function.cpp physical_pivot.cpp physical_unnest.cpp
  physical_prediction_projection.cpp)

target_include_directories(duckdb_operator_projection PUBLIC ${IMLane_INCLUDE_DIRS})

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_operator_projection>
    PARENT_SCOPE)
