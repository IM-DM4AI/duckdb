#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

using duckdb::InvalidInputException;
using duckdb::string;
using duckdb::StringUtil;

namespace duckdb {
enum class PythonUDFKind: uint8_t {COMMON = 0, PREDICTION = 1, PROCESS_PREDICTION = 2, SCHEDULE_PREDICTION = 3};
}