#include "duckdb/common/arrow/arrow_transform_util.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include <iostream>

namespace duckdb {

ExecuteFunctionState::ExecuteFunctionState(const Expression &expr, ExpressionExecutorState &root)
    : ExpressionState(expr, root) {
}

ExecuteFunctionState::~ExecuteFunctionState() {
}

unique_ptr<ExpressionState> ExpressionExecutor::InitializeState(const BoundFunctionExpression &expr,
                                                                ExpressionExecutorState &root, idx_t capacity) {
	auto result = make_uniq<ExecuteFunctionState>(expr, root);
	for (auto &child : expr.children) {
		result->AddChild(child.get(), capacity);
	}
	result->Finalize(false, capacity);
	if (expr.function.init_local_state) {
		result->local_state = expr.function.init_local_state(*result, expr, expr.bind_info.get());
	}
	return std::move(result);
}

static void VerifyNullHandling(const BoundFunctionExpression &expr, DataChunk &args, Vector &result) {
#ifdef DEBUG
	if (args.data.empty() || expr.function.null_handling != FunctionNullHandling::DEFAULT_NULL_HANDLING) {
		return;
	}

	// Combine all the argument validity masks into a flat validity mask
	idx_t count = args.size();
	ValidityMask combined_mask(count);
	for (auto &arg : args.data) {
		UnifiedVectorFormat arg_data;
		arg.ToUnifiedFormat(count, arg_data);

		for (idx_t i = 0; i < count; i++) {
			auto idx = arg_data.sel->get_index(i);
			if (!arg_data.validity.RowIsValid(idx)) {
				combined_mask.SetInvalid(i);
			}
		}
	}

	// Default is that if any of the arguments are NULL, the result is also NULL
	UnifiedVectorFormat result_data;
	result.ToUnifiedFormat(count, result_data);
	for (idx_t i = 0; i < count; i++) {
		if (!combined_mask.RowIsValid(i)) {
			auto idx = result_data.sel->get_index(i);
			D_ASSERT(!result_data.validity.RowIsValid(idx));
		}
	}
#endif
}

void ExpressionExecutor::Execute(const BoundFunctionExpression &expr, ExpressionState *state,
                                 const SelectionVector *sel, idx_t count, Vector &result) {
	state->intermediate_chunk.Reset();
	auto &arguments = state->intermediate_chunk;
	if (!state->types.empty()) {
		for (idx_t i = 0; i < expr.children.size(); i++) {
			D_ASSERT(state->types[i] == expr.children[i]->return_type);
			Execute(*expr.children[i], state->child_states[i].get(), sel, count, arguments.data[i]);
#ifdef DEBUG
			if (expr.children[i]->return_type.id() == LogicalTypeId::VARCHAR) {
				arguments.data[i].UTFVerify(count);
			}
#endif
		}
	}
	arguments.SetCardinality(count);
	arguments.Verify();

	D_ASSERT(expr.function.function);
	if (expr.function.bridge_info && expr.function.bridge_info->kind == FunctionKind::PREDICTION) {
		std::string shm_id = imbridge::thread_id_to_string(std::this_thread::get_id());
		imbridge::SharedMemoryManager shm(shm_id, imbridge::ProcessKind::CLIENT);

		auto table = imbridge::ConvertDataChunkToArrowTable(arguments, context->GetClientProperties());
		imbridge::WriteArrowTableToSharedMemory(table, shm, imbridge::INPUT_TABLE);

		shm.sem_server->post();
		shm.sem_client->wait();

		auto my_table = imbridge::ReadArrowTableFromSharedMemory(shm, imbridge::OUTPUT_TABLE);

		shm.destroy_shared_memory_object<char>(imbridge::INPUT_TABLE);
		shm.destroy_shared_memory_object<char>(imbridge::OUTPUT_TABLE);
		// int cols = my_table->num_columns(), rows = my_table->num_rows();
		// std::vector<int64_t> res;
		// for (int i = 0; i < my_table->num_columns(); i++) {
		// 	std::shared_ptr<arrow::ChunkedArray> column = my_table->column(i);
		// 	for (int chunk_idx = 0; chunk_idx < column->num_chunks(); chunk_idx++) {
		// 		auto chunk = std::static_pointer_cast<arrow::Int32Array>(column->chunk(chunk_idx));
		// 		for (int j = 0; j < chunk->length(); j++) {
		// 			res.push_back(chunk->Value(j));
		// 		}
		// 	}
		// }
		// write result to datachunk
		imbridge::ConvertArrowTableResultToVector(my_table, result);
	} else {
		expr.function.function(arguments, *state, result);
	}
	VerifyNullHandling(expr, arguments, result);
	D_ASSERT(result.GetType() == expr.return_type);
}

} // namespace duckdb
