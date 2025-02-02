#include "imbridge/execution/batch_controller.hpp"

#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/execution/expression_executor.hpp"

namespace duckdb {
namespace imbridge {

template <class T>
static inline data_ptr_t TemplateShift(data_ptr_t ptr, idx_t offset) {
	return reinterpret_cast<data_ptr_t>(reinterpret_cast<T>(ptr) + offset);
}

BatchController::BatchController() : store() {
}

BatchController::~BatchController() {
}

void BatchController::InternalVecShift(Vector &vec, data_ptr_t data_view, idx_t offset) {
	auto &type = vec.GetType();
	switch (type.id()) {
	case LogicalTypeId::BOOLEAN:
		vec.data = TemplateShift<bool *>(data_view, offset);
		break;
	case LogicalTypeId::TINYINT:
		vec.data = TemplateShift<int8_t *>(data_view, offset);
		break;
	case LogicalTypeId::SMALLINT:
		vec.data = TemplateShift<int16_t *>(data_view, offset);
		break;
	case LogicalTypeId::INTEGER:
		vec.data = TemplateShift<int32_t *>(data_view, offset);
		break;
	case LogicalTypeId::DATE:
		vec.data = TemplateShift<date_t *>(data_view, offset);
		break;
	case LogicalTypeId::TIME:
		vec.data = TemplateShift<dtime_t *>(data_view, offset);
		break;
	case LogicalTypeId::TIME_TZ:
		vec.data = TemplateShift<dtime_tz_t *>(data_view, offset);
		break;
	case LogicalTypeId::BIGINT:
		vec.data = TemplateShift<int64_t *>(data_view, offset);
		break;
	case LogicalTypeId::UTINYINT:
		vec.data = TemplateShift<uint8_t *>(data_view, offset);
		break;
	case LogicalTypeId::USMALLINT:
		vec.data = TemplateShift<uint16_t *>(data_view, offset);
		break;
	case LogicalTypeId::UINTEGER:
		vec.data = TemplateShift<uint32_t *>(data_view, offset);
		break;
	case LogicalTypeId::UBIGINT:
		vec.data = TemplateShift<uint64_t *>(data_view, offset);
		break;
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_NS:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_TZ:
		vec.data = TemplateShift<timestamp_t *>(data_view, offset);
		break;
	case LogicalTypeId::HUGEINT:
		vec.data = TemplateShift<hugeint_t *>(data_view, offset);
		break;
	case LogicalTypeId::UHUGEINT:
		vec.data = TemplateShift<uhugeint_t *>(data_view, offset);
		break;
	case LogicalTypeId::UUID:
		vec.data = TemplateShift<hugeint_t *>(data_view, offset);
		break;
	case LogicalTypeId::DECIMAL: {
		switch (type.InternalType()) {
		case PhysicalType::INT16:
			vec.data = TemplateShift<int16_t *>(data_view, offset);
			break;
		case PhysicalType::INT32:
			vec.data = TemplateShift<int32_t *>(data_view, offset);
			break;
		case PhysicalType::INT64:
			vec.data = TemplateShift<int64_t *>(data_view, offset);
			break;
		case PhysicalType::INT128:
			vec.data = TemplateShift<hugeint_t *>(data_view, offset);
			break;
		default:
			throw InternalException("Physical type '%s' has a width bigger than 38, which is not supported",
			                        TypeIdToString(type.InternalType()));
		}
		break;
	}
	case LogicalTypeId::ENUM: {
		switch (type.InternalType()) {
		case PhysicalType::UINT8:
			vec.data = TemplateShift<uint8_t *>(data_view, offset);
			break;
		case PhysicalType::UINT16:
			vec.data = TemplateShift<uint16_t *>(data_view, offset);
			break;
		case PhysicalType::UINT32:
			vec.data = TemplateShift<uint32_t *>(data_view, offset);
			break;
		default:
			throw InternalException("ENUM can only have unsigned integers as physical types");
		}
		break;
	}
	case LogicalTypeId::POINTER:
		vec.data = TemplateShift<uintptr_t *>(data_view, offset);
		break;
	case LogicalTypeId::FLOAT:
		vec.data = TemplateShift<float *>(data_view, offset);
		break;
	case LogicalTypeId::DOUBLE:
		vec.data = TemplateShift<double *>(data_view, offset);
		break;
	case LogicalTypeId::INTERVAL:
		vec.data = TemplateShift<interval_t *>(data_view, offset);
		break;
	case LogicalTypeId::VARCHAR:
	case LogicalTypeId::BLOB:
	case LogicalTypeId::AGGREGATE_STATE:
	case LogicalTypeId::BIT:
		vec.data = TemplateShift<string_t *>(data_view, offset);
		break;
	case LogicalTypeId::MAP:
		vec.data = TemplateShift<list_entry_t *>(data_view, offset);
		break;
	case LogicalTypeId::UNION:
	case LogicalTypeId::STRUCT:
	case LogicalTypeId::LIST:
	case LogicalTypeId::ARRAY:
	default:
		throw NotImplementedException("Unimplemented type '%s' for buffer chunk slicing!",
		                              TypeIdToString(vec.GetType().InternalType()));
	}
}

void BatchController::InternalSlicing(DataChunk &source, DataChunk &target, idx_t start_offset, idx_t stop_offset) {
	D_ASSERT(stop_offset > start_offset);
	// D_ASSERT(stop_offset <= high_offset);
	idx_t slice_size = stop_offset - start_offset;
	auto sel = FlatVector::IncrementalSelectionVector();

	for (idx_t i = 0; i < source.ColumnCount(); i++) {
		auto &column_vec = source.data[i];
		auto &curr_mask = column_vec.validity;
		auto &data_view = column_vec.data;

		// first, get a new sliced validarity mask
		ValidityMask slice_mask = ValidityMask(slice_size);
		slice_mask.CopySel(curr_mask, *sel, start_offset, 0, slice_size);
		target.data[i].validity = slice_mask;

		// then, slice the actual vector data
		// assume that the data types are not composite types, e.g., union, struct..
		// since their data addressing modes behave differently
		InternalVecShift(target.data[i], data_view, start_offset);
	}

	target.SetCardinality(slice_size);
}

void BatchController::Initialize(Allocator &allocator, const vector<LogicalType> &types, idx_t capacity) {
	base_offset = 0;
	high_offset = base_offset;
	state = BatchControllerState::EMPTY;
	store.Initialize(allocator, types, capacity);
	sliced.InitializeEmpty(types);
	sliced.capacity = capacity;

	for (idx_t i = 0; i < store.ColumnCount(); i++) {

		// no need for "buffer" sharing, since the buffer store chunk is always
		// a chunk consisting of all the flat vectors
		AssignSharedPointer(sliced.data[i].auxiliary, store.data[i].auxiliary);
	}
}

void BatchController::ResetBuffer() {
	base_offset = 0;
	high_offset = base_offset;
	state = BatchControllerState::EMPTY;
	sliced.Reset();
	sliced.capacity = store.capacity;

	if (store.data.empty() || store.vector_caches.empty()) {
		return;
	}
	if (store.vector_caches.size() != store.data.size()) {
		throw InternalException("VectorCache and column count mismatch in ChunkBuffer::ResetBuffer");
	}
	for (idx_t i = 0; i < store.ColumnCount(); i++) {
		store.data[i].ResetFromCache(store.vector_caches[i]);

		AssignSharedPointer(sliced.data[i].auxiliary, store.data[i].auxiliary);
	}
	store.SetCardinality(0);
}

void BatchController::PushChunk(ClientContext &context, const DataChunk &other, idx_t start_offset, idx_t stop_offset) {
	D_ASSERT(state != BatchControllerState::SLICING);
	idx_t count = stop_offset - start_offset;
	idx_t new_offset = high_offset + count;
	if (count == 0) {
		return;
	}
	if (store.ColumnCount() != other.ColumnCount()) {
		throw InternalException("Column counts of appending chunk doesn't match!");
	}

	if (new_offset > store.capacity) {
		auto new_capacity = NextPowerOfTwo(new_offset);
		for (idx_t i = 0; i < store.ColumnCount(); i++) {
			store.data[i].Resize(high_offset, new_capacity);
			AssignSharedPointer(sliced.data[i].auxiliary, store.data[i].auxiliary);
			store.ResizeCache(context, i, new_capacity);
		}
		store.capacity = new_capacity;
		sliced.capacity = new_capacity;
	}
	for (idx_t i = 0; i < store.ColumnCount(); i++) {
		D_ASSERT(store.data[i].GetVectorType() == VectorType::FLAT_VECTOR);
		VectorOperations::Copy(other.data[i], store.data[i], stop_offset, start_offset, high_offset);
	}

	high_offset = new_offset;
}

void BatchController::PushChunk(ClientContext &context, const DataChunk &other) {
	PushChunk(context, other, 0, other.size());
}

DataChunk &BatchController::NextBatch(idx_t required) {
	idx_t start_offset = base_offset;
	idx_t stop_offset = start_offset + required;
	InternalSlicing(store, sliced, start_offset, stop_offset);
	base_offset += required;
	return sliced;
}

bool BatchController::HasNext(idx_t required) {
	return high_offset >= base_offset + required;
}

BatchControllerState BatchController::GetState() {
	return state;
}

void BatchController::SetState(BatchControllerState new_state) {
	state = new_state;
}

idx_t BatchController::GetSize() {
	return high_offset - base_offset;
}

// helper methods
void BatchController::ExternalProjectionReset(ClientContext &context, DataChunk &input, ExpressionExecutor &executor) {
	for (idx_t i = 0; i < input.ColumnCount(); i++) {
		input.data[i].ResetFromCache(input.vector_caches[i]);
	}
	if (input.capacity < store.capacity) {
		auto new_capacity = store.capacity;
		for (idx_t i = 0; i < input.ColumnCount(); i++) {
			input.data[i].Resize(0, new_capacity);
			input.ResizeCache(context, i, new_capacity);
		}
		input.capacity = store.capacity;

		// resize all the intermediate chunks in executor
		auto &executor_states = executor.GetStates();
		for (auto &executor_state : executor_states) {
			executor_state->root_state->UpdateCapacity(context, store.capacity);
		}
	}
}

void BatchController::BatchAdapting(DataChunk &input, DataChunk &output, idx_t start_offset, idx_t size) {
	idx_t stop_offset = start_offset + size;
	InternalSlicing(input, output, start_offset, stop_offset);
}

unique_ptr<DataChunk> BatchController::AsyncExecuteExpression(ClientContext &context, ExpressionExecutor &executor,
                                                              unique_ptr<DataChunk> input,
                                                              vector<LogicalType> return_types, idx_t buffer_size) {
	unique_ptr<DataChunk> out_buffer = make_uniq<DataChunk>();
	out_buffer->Initialize(Allocator::Get(context), return_types, buffer_size);
	unique_ptr<ExpressionExecutor> new_executor = executor.Copy();
	auto &executor_states = new_executor->GetStates();
	for (auto &executor_state : executor_states) {
		executor_state->root_state->UpdateCapacity(context, input->GetCapacity());
	}
	new_executor->Execute(*input, *out_buffer);
	return out_buffer;
}

} // namespace imbridge

} // namespace duckdb