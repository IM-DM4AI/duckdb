#include "duckdb/execution/operator/projection/physical_projection.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "duckdb/planner/operator/logical_projection.hpp"
#include "imbridge/execution/operator/physical_prediction_projection.hpp"
#include "imbridge/execution/plan_prediction_util.hpp"

namespace duckdb {

using namespace imbridge;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalProjection &op) {
	D_ASSERT(op.children.size() == 1);
	auto plan = CreatePlan(*op.children[0]);

#ifdef DEBUG
	for (auto &expr : op.expressions) {
		D_ASSERT(!expr->IsWindow());
		D_ASSERT(!expr->IsAggregate());
	}
#endif
	if (plan->types.size() == op.types.size()) {
		// check if this projection can be omitted entirely
		// this happens if a projection simply emits the columns in the same order
		// e.g. PROJECTION(#0, #1, #2, #3, ...)
		bool omit_projection = true;
		for (idx_t i = 0; i < op.types.size(); i++) {
			if (op.expressions[i]->type == ExpressionType::BOUND_REF) {
				auto &bound_ref = op.expressions[i]->Cast<BoundReferenceExpression>();
				if (bound_ref.index == i) {
					continue;
				}
			}
			omit_projection = false;
			break;
		}
		if (omit_projection) {
			// the projection only directly projects the child' columns: omit it entirely
			return plan;
		}
	}

	// IMBridge optimization: check the expression list
	// try to extract prediction function and transform it as a standalone physical projection operator
	// Optimization condition： only one prediction function appears in "op.expressions"
	PredictionFuncChecker func_checker(op.expressions);

	if (func_checker.CheckExprs([&](idx_t count) { return count == 1; })) {
		D_ASSERT(func_checker.kind == FunctionKind::BATCH_PREDICTION ||
		         func_checker.kind == FunctionKind::ASYNC_PREDICTION ||
		         func_checker.kind == FunctionKind::SCHEDULE_PREDICTION ||
				 func_checker.kind == FunctionKind::ASYNC_BATCH_PREDICTION);
		auto root_idx = *func_checker.root_idx_list.begin();
		auto prediction_size = func_checker.user_batch_size_map[root_idx];

		auto prediction_projection = make_uniq<PhysicalPredictionProjection>(
		    op.types, std::move(op.expressions), op.estimated_cardinality, prediction_size, func_checker.kind);
		prediction_projection->children.push_back(std::move(plan));
		return std::move(prediction_projection);
	}

	// Fallback to the normal plan generation path
	auto projection = make_uniq<PhysicalProjection>(op.types, std::move(op.expressions), op.estimated_cardinality);
	projection->children.push_back(std::move(plan));
	return std::move(projection);
}

} // namespace duckdb
