//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/arrow/arrow_transform_util.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once
#include "duckdb/common/arrow/arrow.hpp"
#include "imlane/ipc/shared_memory_manager.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/main/client_properties.hpp"

#include <arrow/api.h>
#include <boost/interprocess/managed_shared_memory.hpp>

namespace bi = boost::interprocess;

namespace duckdb {
namespace imbridge {

const std::string INPUT_TABLE = "INPUT_TABLE";
const std::string OUTPUT_TABLE = "OUTPUT_TABLE";

std::shared_ptr<arrow::Table> ConvertDataChunkToArrowTable(DataChunk &input, const ClientProperties &options);

void WriteArrowTableToSharedMemory(std::shared_ptr<arrow::Table> &table, SharedMemoryManager &shm,
                                   const std::string &shm_id = INPUT_TABLE);

std::shared_ptr<arrow::Table> ReadArrowTableFromSharedMemory(SharedMemoryManager &shm,
                                                             const std::string &shm_id = OUTPUT_TABLE);

void ConvertArrowTableResultToVector(std::shared_ptr<arrow::Table> &table, Vector &res);

} // namespace imbridge
} // namespace duckdb
