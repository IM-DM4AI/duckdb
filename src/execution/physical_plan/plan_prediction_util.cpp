#include "imbridge/execution/plan_prediction_util.hpp"

#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/planner/expression_iterator.hpp"

namespace duckdb {

namespace imbridge {

bool PredictionFuncChecker::CheckExprs(std::function<bool(idx_t)> constraint) {
	total_prediction_func_count = 0;
	user_batch_size_map.assign(expressions.size(), 0);
	root_idx_list.clear();

	for (idx_t i = 0; i < expressions.size(); i++) {
		VisitExpression(&expressions[i], i);
	}
	return constraint(total_prediction_func_count);
}

void PredictionFuncChecker::VisitExpression(unique_ptr<Expression> *expression, idx_t root_idx) {
	auto &expr = **expression;
	if (expr.GetExpressionClass() == ExpressionClass::BOUND_FUNCTION) {
		auto &func_expr = expr.Cast<BoundFunctionExpression>();
		if (func_expr.function.bridge_info) {
			if (func_expr.function.bridge_info->kind == FunctionKind::BATCH_PREDICTION ||
			    func_expr.function.bridge_info->kind == FunctionKind::ASYNC_PREDICTION ||
			    func_expr.function.bridge_info->kind == FunctionKind::SCHEDULE_PREDICTION || 
				func_expr.function.bridge_info->kind == FunctionKind::ASYNC_BATCH_PREDICTION ) {
				kind = func_expr.function.bridge_info->kind;
				total_prediction_func_count += 1;
				user_batch_size_map[root_idx] =
				    std::max(idx_t(func_expr.function.bridge_info->batch_size), user_batch_size_map[root_idx]);
				root_idx_list.insert(root_idx);
			}
		}
	}
	VisitExpressionChild(expr, root_idx);
}

void PredictionFuncChecker::VisitExpressionChild(Expression &expr, idx_t root_idx) {
	ExpressionIterator::EnumerateChildren(expr,
	                                      [&](unique_ptr<Expression> &expr) { VisitExpression(&expr, root_idx); });
}

} // namespace imbridge

} // namespace duckdb