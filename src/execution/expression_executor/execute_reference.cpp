#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"

namespace duckdb {

unique_ptr<ExpressionState> ExpressionExecutor::InitializeState(const BoundReferenceExpression &expr,
                                                                ExpressionExecutorState &root, idx_t capacity) {

	auto result = make_uniq<ExpressionState>(expr, root);
	auto &flags = root.executor->sub_expr_eval_flags;
	flags.push_back(0);
	result->eval_flag_idx = flags.size() - 1;
	
	result->Finalize(true, capacity);
	return result;
}

void ExpressionExecutor::Execute(const BoundReferenceExpression &expr, ExpressionState *state,
                                 const SelectionVector *sel, idx_t count, Vector &result) {
	D_ASSERT(expr.index != DConstants::INVALID_INDEX);
	D_ASSERT(expr.index < chunk->ColumnCount());

	if (sel) {
		result.Slice(chunk->data[expr.index], *sel, count);
	} else {
		result.Reference(chunk->data[expr.index]);
	}

	state->SetEvaluated();
}

} // namespace duckdb
