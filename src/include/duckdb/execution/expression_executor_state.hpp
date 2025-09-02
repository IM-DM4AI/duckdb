//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/expression_executor_state.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/function/function.hpp"

namespace IMLane {
	namespace DBEnd {
		template<typename ARGS_TYPE, typename RET_TYPE>
		class ExecFuncContext;
	}
}


namespace duckdb {
class Expression;
class ExpressionExecutor;
struct ExpressionExecutorState;
struct FunctionLocalState;

struct ExpressionState {
	ExpressionState(const Expression &expr, ExpressionExecutorState &root);
	virtual ~ExpressionState() {
	}

	const Expression &expr;
	ExpressionExecutorState &root;
	vector<unique_ptr<ExpressionState>> child_states;
	vector<LogicalType> types;
	DataChunk intermediate_chunk;

	// IMLane optimization: indicate wheather expr with this state
	// is evaluated to avoid re-computation during udf result pull.
	idx_t eval_flag_idx;
public:
	bool IsEvaluated();
	void SetEvaluated();
	void ResetEvaluated();

public:
	void AddChild(Expression *expr, idx_t capacity = STANDARD_VECTOR_SIZE);
	void Finalize(bool empty = false, idx_t capacity = STANDARD_VECTOR_SIZE);
	void UpdateCapacity(idx_t capacity);
	Allocator &GetAllocator();
	bool HasContext();
	DUCKDB_API ClientContext &GetContext();

	void Verify(ExpressionExecutorState &root);

public:
	template <class TARGET>
	TARGET &Cast() {
		DynamicCastCheck<TARGET>(this);
		return reinterpret_cast<TARGET &>(*this);
	}
	template <class TARGET>
	const TARGET &Cast() const {
		DynamicCastCheck<TARGET>(this);
		return reinterpret_cast<const TARGET &>(*this);
	}
};

struct ExecuteFunctionState : public ExpressionState {
	ExecuteFunctionState(const Expression &expr, ExpressionExecutorState &root);
	~ExecuteFunctionState() override;

	unique_ptr<FunctionLocalState> local_state;

	shared_ptr<IMLane::DBEnd::ExecFuncContext<DataChunk, Vector>> exec_ctx;

public:
	static optional_ptr<FunctionLocalState> GetFunctionState(ExpressionState &state) {
		return state.Cast<ExecuteFunctionState>().local_state.get();
	}
};

struct ExpressionExecutorState {
	ExpressionExecutorState();

	unique_ptr<ExpressionState> root_state;
	ExpressionExecutor *executor = nullptr;

	void Verify();
};

} // namespace duckdb
