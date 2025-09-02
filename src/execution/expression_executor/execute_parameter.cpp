#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_parameter_expression.hpp"

namespace duckdb {

unique_ptr<ExpressionState> ExpressionExecutor::InitializeState(const BoundParameterExpression &expr,
                                                                ExpressionExecutorState &root, idx_t capacity) {
	auto result = make_uniq<ExpressionState>(expr, root);
	auto &flags = root.executor->sub_expr_eval_flags;
	flags.push_back(0);
	result->eval_flag_idx = flags.size() - 1;
	
	result->Finalize(false, capacity);

	return result;
}

void ExpressionExecutor::Execute(const BoundParameterExpression &expr, ExpressionState *state,
                                 const SelectionVector *sel, idx_t count, Vector &result) {
	D_ASSERT(expr.parameter_data);
	D_ASSERT(expr.parameter_data->return_type == expr.return_type);
	D_ASSERT(expr.parameter_data->GetValue().type() == expr.return_type);
	result.Reference(expr.parameter_data->GetValue());

	state->SetEvaluated();
}

} // namespace duckdb
