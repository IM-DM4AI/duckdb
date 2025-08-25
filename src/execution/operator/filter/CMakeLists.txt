add_library_unity(duckdb_operator_filter OBJECT physical_filter.cpp physical_prediction_filter.cpp)

target_include_directories(duckdb_operator_filter PUBLIC ${IMLane_INCLUDE_DIRS})

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_operator_filter>
    PARENT_SCOPE)
