#pragma once
#include "duckdb/common/vector.hpp"
#include "duckdb/common/allocator.hpp"
#include "duckdb/common/types/validity_mask.hpp"
#include "duckdb/common/types/data_chunk.hpp"

namespace duckdb {

class ExpressionExecutor;

namespace imbridge {

enum class BatchControllerState: uint8_t { EMPTY, SLICING, BUFFERRING };
    
class BatchController {
public:
	BatchController();
	~BatchController();
    void Initialize(Allocator &allocator, const vector<LogicalType> &types, idx_t capacity);
    void ResetBuffer();
    void PushChunk(ClientContext &context, const DataChunk &other);
    void PushChunk(ClientContext &context, const DataChunk &other, idx_t start_offset, idx_t count);
    DataChunk & NextBatch(idx_t required);
    bool HasNext(idx_t required);
public:
    BatchControllerState GetState();
    void SetState(BatchControllerState new_state);
    idx_t GetSize();
public:
    // helper method for external projection state reset
    void ExternalProjectionReset(ClientContext &context, DataChunk &input, ExpressionExecutor &executor);
    // helper method for batch adpater, keep the output batch size <= STANDARD_VECTOR_SIZE
    void BatchAdapting(DataChunk &input, DataChunk &output, idx_t start_offset, idx_t size=STANDARD_VECTOR_SIZE);
    // async execute expression
    unique_ptr<DataChunk> AsyncExecuteExpression(ClientContext &context, ExpressionExecutor &executor, unique_ptr<DataChunk> input, vector<LogicalType> return_types, idx_t buffer_size = STANDARD_VECTOR_SIZE);

private:
    void InternalVecShift(Vector &vec, data_ptr_t data_view, idx_t offset);
    void InternalSlicing(DataChunk &source, DataChunk &target, idx_t low, idx_t high);
private:
    DataChunk store;
    DataChunk sliced;
    idx_t base_offset;
    idx_t high_offset;
    BatchControllerState state;
};

} // namespace imbridge

} // namespace duckdb
