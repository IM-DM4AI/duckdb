#include "duckdb_python/functional.hpp"

namespace duckdb {

void DuckDBPyFunctional::Initialize(py::module_ &parent) {
	auto m =
	    parent.def_submodule("functional", "This module contains classes and methods related to functions and udf");

	py::enum_<duckdb::PythonUDFType>(m, "PythonUDFType")
	    .value("NATIVE", duckdb::PythonUDFType::NATIVE)
	    .value("ARROW", duckdb::PythonUDFType::ARROW)
	    .export_values();

	py::enum_<duckdb::FunctionNullHandling>(m, "FunctionNullHandling")
	    .value("DEFAULT", duckdb::FunctionNullHandling::DEFAULT_NULL_HANDLING)
	    .value("SPECIAL", duckdb::FunctionNullHandling::SPECIAL_HANDLING)
	    .export_values();

	py::enum_<duckdb::PythonUDFKind>(m, "PythonUDFKind")
		.value("COMMON", duckdb::PythonUDFKind::COMMON)
		.value("PREDICTION", duckdb::PythonUDFKind::PREDICTION)
		.value("PROCESS_PREDICTION", duckdb::PythonUDFKind::PROCESS_PREDICTION)
		.value("SCHEDULE_PREDICTION", duckdb::PythonUDFKind::SCHEDULE_PREDICTION)
		.value("THREAD_SCHEDULE_PREDICTION", duckdb::PythonUDFKind::THREAD_SCHEDULE_PREDICTION)
		.export_values();
}

} // namespace duckdb
