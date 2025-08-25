#include "duckdb/execution/operator/filter/physical_filter.hpp"
#include "duckdb/execution/operator/projection/physical_projection.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/optimizer/matcher/expression_matcher.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "duckdb/planner/operator/logical_filter.hpp"
#include "duckdb/planner/operator/logical_get.hpp"

#include "prediction/execution/operator/physical_prediction_filter.hpp"
#include "prediction/plan_prediction_util.hpp"

namespace duckdb {

using namespace prediction;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalFilter &op) {
	D_ASSERT(op.children.size() == 1);
	unique_ptr<PhysicalOperator> plan = CreatePlan(*op.children[0]);
	if (!op.expressions.empty()) {
		D_ASSERT(plan->types.size() > 0);
		// create a filter if there is anything to filter

		// Prediction optimization: check the predicate expressions
		// try to extract the prediction function within a predicate and lift it as a standalone physical filter
		// Optimization condition： only one prediction function appears in "op.expressions" (children of the conjunction predicate)
		PredictionFuncChecker func_checker(op.expressions);

		if(func_checker.CheckExprs([&](idx_t count){return count == 1;})) {
			auto root_idx = *func_checker.root_idx_list.begin();
			auto prediction_size = func_checker.user_batch_size_map[root_idx];

			if(op.expressions.size() == 1) {
				// only one expression, just turn it into prediction filter
				auto prediction_filter =  make_uniq<PhysicalPredictionFilter>(plan->types, std::move(op.expressions),
				op.estimated_cardinality, prediction_size, func_checker.kind);
				prediction_filter->children.push_back(std::move(plan));
				plan = std::move(prediction_filter);
			} else {
				// multiple expressions, lift the prediction function related expressions
				vector<unique_ptr<Expression>> lifted_exprs;
				vector<unique_ptr<Expression>> remained_exprs;

				for(idx_t i = 0; i < op.expressions.size(); i++) {
					if(i == root_idx) {
						lifted_exprs.push_back(std::move(op.expressions[i]));
					} else {
						remained_exprs.push_back(std::move(op.expressions[i]));
					}
				}
				auto lifted_filter = make_uniq<PhysicalPredictionFilter>(plan->types, std::move(lifted_exprs),
				op.estimated_cardinality, prediction_size, func_checker.kind);
				auto remained_filter = make_uniq<PhysicalFilter>(plan->types, std::move(remained_exprs), op.estimated_cardinality);

				remained_filter->children.push_back(std::move(plan));
				plan = std::move(remained_filter);
				lifted_filter->children.push_back(std::move(plan));
				plan = std::move(lifted_filter);
			}
		} else {
			// Fallback to the normal plan generation path	
			auto filter = make_uniq<PhysicalFilter>(plan->types, std::move(op.expressions), op.estimated_cardinality);
			filter->children.push_back(std::move(plan));
			plan = std::move(filter);
		}
	}
	if (!op.projection_map.empty()) {
		// there is a projection map, generate a physical projection
		vector<unique_ptr<Expression>> select_list;
		for (idx_t i = 0; i < op.projection_map.size(); i++) {
			select_list.push_back(make_uniq<BoundReferenceExpression>(op.types[i], op.projection_map[i]));
		}
		auto proj = make_uniq<PhysicalProjection>(op.types, std::move(select_list), op.estimated_cardinality);
		proj->children.push_back(std::move(plan));
		plan = std::move(proj);
	}
	return plan;
}

} // namespace duckdb
