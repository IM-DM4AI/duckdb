#include "duckdb/common/arrow/arrow_transform_util.hpp"

#include "duckdb/common/arrow/arrow_appender.hpp"
#include "duckdb/common/arrow/arrow_converter.hpp"
#include "duckdb/common/exception/conversion_exception.hpp"
#include "duckdb/common/types/arrow_string_view_type.hpp"

#include <arrow/array/concatenate.h>
#include <arrow/c/abi.h>
#include <arrow/c/bridge.h>
#include <arrow/c/helpers.h>
#include <arrow/io/api.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/writer.h>
#include <arrow/status.h>
#include <string>
#include <thread>
#include <iostream>
#include <sched.h>

namespace duckdb {

namespace imbridge {

std::shared_ptr<arrow::Table> ConvertDataChunkToArrowTable(DataChunk &input, const ClientProperties &options) {
	auto types = input.GetTypes();
	vector<string> names;
	names.reserve(types.size());

	for (idx_t i = 0; i < types.size(); i++) {
		names.push_back(StringUtil::Format("c%d", i));
	}

	ArrowSchema schema;
	ArrowConverter::ToArrowSchema(&schema, types, names, options);

	idx_t init_capacity = input.size() > STANDARD_VECTOR_SIZE ? NextPowerOfTwo(input.size()) : STANDARD_VECTOR_SIZE;
	ArrowAppender appender(types, init_capacity, options);
	appender.Append(input, 0, input.size(), input.size());
	ArrowArray array = appender.Finalize();

	std::shared_ptr<arrow::Schema> arrow_schema;
	std::vector<std::shared_ptr<arrow::Array>> arrow_array;

	for (int i = 0; i < array.n_children; i++) {
		arrow_array.emplace_back(arrow::ImportArray(array.children[i], schema.children[i]).ValueOrDie());
	}

	ArrowSchema schema1;
	ArrowConverter::ToArrowSchema(&schema1, types, names, options);

	arrow_schema = arrow::ImportSchema(&schema1).ValueOrDie();

	std::shared_ptr<arrow::Table> table = arrow::Table::Make(arrow_schema, arrow_array);
	return table;
}

void WriteArrowTableToSharedMemory(std::shared_ptr<arrow::Table> &table, SharedMemoryManager &shm,
                                   const std::string &shm_id) {
	std::shared_ptr<arrow::Buffer> buffer;
	int col = table->num_columns(), row = table->num_rows();
	auto bit = sizeof(double_t);
	std::shared_ptr<arrow::io::BufferOutputStream> stream =
	    arrow::io::BufferOutputStream::Create(table->num_columns() * table->num_rows() * sizeof(double_t)).ValueOrDie();
	std::shared_ptr<arrow::ipc::RecordBatchWriter> writer =
	    arrow::ipc::MakeStreamWriter(stream, table->schema()).ValueOrDie();
	writer->WriteTable(*table);
	writer->Close();
	buffer = stream->Finish().ValueOrDie();

	char *shm_ptr = shm.create_shared_memory_object<char>(shm_id, buffer->size());
	std::memcpy(shm_ptr, buffer->data(), buffer->size());
	int* cpu_id = shm.find_or_construct_shared_memory_object<int>("cpu_id", -1);
	*cpu_id = sched_getcpu();
	// std::cout<<"write db thread id : "<< std::this_thread::get_id() <<"   cpu core: " << sched_getcpu() << "          "<< *cpu_id  << std::endl;
}

std::shared_ptr<arrow::Table> ReadArrowTableFromSharedMemory(SharedMemoryManager &shm, const std::string &shm_id) {
	auto shm_table_pair = shm.open_shared_memory_object<char>(shm_id);
	int* cpu_id = shm.find_or_construct_shared_memory_object<int>("cpu_id", -1);
	// std::cout<<"read db thread id : "<< std::this_thread::get_id() <<"   cpu core: " << sched_getcpu() << "          "<< *cpu_id  << std::endl;
	if (shm_table_pair.first == nullptr) {
		throw std::runtime_error("[BOOST SHARED MEMORY] Cannot find shared memory with id: " + shm.get_channel_name() +
		                         shm_id);
		return nullptr;
	}

	char *shm_table_ptr = shm_table_pair.first;
	size_t shm_table_size = shm_table_pair.second;

	std::shared_ptr<arrow::Buffer> buffer = arrow::Buffer::Wrap(shm_table_ptr, shm_table_size);
	std::shared_ptr<arrow::io::InputStream> input = std::make_shared<arrow::io::BufferReader>(buffer);
	std::shared_ptr<arrow::ipc::RecordBatchReader> reader =
	    arrow::ipc::RecordBatchStreamReader::Open(input).ValueOrDie();

	std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
	while (true) {
		std::shared_ptr<arrow::RecordBatch> batch = reader->ReadNext().ValueOrDie().batch;
		if (batch == nullptr) {
			break;
		}
		batches.push_back(batch);
	}

	std::shared_ptr<arrow::Table> table = arrow::Table::FromRecordBatches(reader->schema(), batches).ValueOrDie();
	return table;
}

void ConvertArrowTableResultToVector(std::shared_ptr<arrow::Table> &table, Vector &res) {
	// As the duckdb_python_udf, UDF only support one column return.
	// only support directyly conver
	idx_t size = table->num_rows();
	std::shared_ptr<arrow::ChunkedArray> column = table->column(0);
	std::vector<std::shared_ptr<arrow::Array>> chunks = column->chunks();
	std::shared_ptr<arrow::Array> array = arrow::Concatenate(chunks).ValueOrDie();
	ArrowArray c_array;
	ArrowSchema c_array_type;
	arrow::ExportArray(*array, &c_array);
	arrow::ExportType(*array->type(), &c_array_type);
	std::string ctype(c_array_type.format);
	switch (res.GetType().id()) {
	case LogicalTypeId::BOOLEAN:
	case LogicalTypeId::TINYINT:
	case LogicalTypeId::SMALLINT:
	case LogicalTypeId::INTEGER:
	case LogicalTypeId::FLOAT:
	case LogicalTypeId::DOUBLE:
	case LogicalTypeId::UTINYINT:
	case LogicalTypeId::USMALLINT:
	case LogicalTypeId::UINTEGER:
	case LogicalTypeId::UBIGINT:
	case LogicalTypeId::BIGINT:
	case LogicalTypeId::HUGEINT:
	case LogicalTypeId::UHUGEINT:
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP_NS: {
		auto data_ptr = (data_ptr_t)c_array.buffers[1];
		FlatVector::SetData(res, data_ptr);
		break;
	}
	case LogicalTypeId::VARCHAR: {
		if (ctype == "u" || ctype == "z") { // NORMAL FIXED
			auto c_data = (char *)c_array.buffers[2];
			auto offsets = (uint32_t *)c_array.buffers[1];
			auto strings = FlatVector::GetData<string_t>(res);
			for (idx_t row_idx = 0; row_idx < size; row_idx++) {
				if (FlatVector::IsNull(res, row_idx)) {
					continue;
				}
				auto cptr = c_data + offsets[row_idx];
				auto str_len = offsets[row_idx + 1] - offsets[row_idx];
				if (str_len > NumericLimits<uint32_t>::Maximum()) { // LCOV_EXCL_START
					throw duckdb::ConversionException("DuckDB does not support Strings over 4GB");
				} // LCOV_EXCL_STOP
				strings[row_idx] = string_t(cptr, UnsafeNumericCast<uint32_t>(str_len));
			}
		} else if (ctype == "U" || ctype == "Z") { // SUPER
			auto c_data = (char *)c_array.buffers[2];
			auto offsets = (uint64_t *)c_array.buffers[1];
			auto strings = FlatVector::GetData<string_t>(res);
			for (idx_t row_idx = 0; row_idx < size; row_idx++) {
				if (FlatVector::IsNull(res, row_idx)) {
					continue;
				}
				auto cptr = c_data + offsets[row_idx];
				auto str_len = offsets[row_idx + 1] - offsets[row_idx];
				if (str_len > NumericLimits<uint32_t>::Maximum()) { // LCOV_EXCL_START
					throw duckdb::ConversionException("DuckDB does not support Strings over 4GB");
				} // LCOV_EXCL_STOP
				strings[row_idx] = string_t(cptr, UnsafeNumericCast<uint32_t>(str_len));
			}
		} else if (ctype == "vu") { // VIEW
			auto strings = FlatVector::GetData<string_t>(res);
			auto arrow_string = (arrow_string_view_t *)c_array.buffers[1];
			for (idx_t row_idx = 0; row_idx < size; row_idx++) {
				if (FlatVector::IsNull(res, row_idx)) {
					continue;
				}
				auto length = UnsafeNumericCast<uint32_t>(arrow_string[row_idx].Length());
				if (arrow_string[row_idx].IsInline()) {
					strings[row_idx] = string_t(arrow_string[row_idx].GetInlineData(), length);
				} else {
					auto buffer_index = UnsafeNumericCast<uint32_t>(arrow_string[row_idx].GetBufferIndex());
					int32_t offset = arrow_string[row_idx].GetOffset();
					D_ASSERT(c_array.n_buffers > 2 + buffer_index);
					auto c_data = (char *)c_array.buffers[2 + buffer_index];
					strings[row_idx] = string_t(&c_data[offset], length);
				}
			}
		} else {
			throw duckdb::ConversionException("Unsupported Arrow String format: %s", c_array_type.format);
		}

		break;
	}
	default:
		throw NotImplementedException("Unsupported type for arrow conversion: %s", res.GetType().ToString());
	}
}

} // namespace imbridge

} // namespace duckdb