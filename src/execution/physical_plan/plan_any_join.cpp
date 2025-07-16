#include "duckdb/execution/operator/join/physical_blockwise_nl_join.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/planner/operator/logical_any_join.hpp"

#include "imbridge/execution/operator/physical_prediction_filter.hpp"
#include "imbridge/execution/plan_prediction_util.hpp"
#include "duckdb/execution/operator/filter/physical_filter.hpp"
#include "duckdb/planner/operator/logical_filter.hpp"
#include "duckdb/execution/operator/join/physical_cross_product.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"

namespace duckdb {

using namespace imbridge;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalAnyJoin &op) {
	// first visit the child nodes
	D_ASSERT(op.children.size() == 2);
	D_ASSERT(op.condition);

	auto left = CreatePlan(*op.children[0]);
	auto right = CreatePlan(*op.children[1]);

	unique_ptr<PhysicalOperator> plan;


	// IMBridge optimization: check the join condition
	// try to extract the prediction function within a predicate and lift it as a standalone physical filter
	// Optimization conditions： 
	// 1. only one prediction function appears in "op.condition" (split into children of the conjunction predicates)
	// 2. only perform on inner join
	// remove the prediction function related condition, if conditions become empty after extraction, perform crossproduct join.
	vector<unique_ptr<Expression>> conditions;
	conditions.push_back(std::move(op.condition));
	LogicalFilter::SplitPredicates(conditions);

	PredictionFuncChecker func_checker(conditions);

	if(func_checker.CheckExprs([&](idx_t count){return count == 1 && op.join_type == JoinType::INNER;})) {
		auto root_idx = *func_checker.root_idx_list.begin();
		auto prediction_size = func_checker.user_batch_size_map[root_idx];

		if(conditions.size() == 1) {
			// only one expression, perform cross product join and add a prediction filter
			auto prediction_filter =  make_uniq<PhysicalPredictionFilter>(op.types, std::move(conditions),
			op.estimated_cardinality, prediction_size, func_checker.kind);
			plan = make_uniq<PhysicalCrossProduct>(op.types, std::move(left), std::move(right), op.estimated_cardinality);
			
			prediction_filter->children.push_back(std::move(plan));
			plan = std::move(prediction_filter);
		} else {
			// multiple expressions, lift the prediction function related expressions
			vector<unique_ptr<Expression>> lifted_exprs;
			vector<unique_ptr<Expression>> remained_exprs;

			for(idx_t i = 0; i < conditions.size(); i++) {
				if(i == root_idx) {
					lifted_exprs.push_back(std::move(conditions[i]));
				} else {
					remained_exprs.push_back(std::move(conditions[i]));
				}
			}

			op.condition = std::move(remained_exprs[0]);
			for (idx_t i = 1; i < remained_exprs.size(); i++) {
				op.condition = make_uniq<BoundConjunctionExpression>(
					ExpressionType::CONJUNCTION_AND, std::move(op.condition), std::move(remained_exprs[i]));
			}
			auto remained_filter_join = make_uniq<PhysicalBlockwiseNLJoin>(op, std::move(left), std::move(right), std::move(op.condition),
	                                          op.join_type, op.estimated_cardinality);
			plan = std::move(remained_filter_join);


			auto lifted_filter = make_uniq<PhysicalPredictionFilter>(op.types, std::move(lifted_exprs),
			op.estimated_cardinality, prediction_size, func_checker.kind);
			lifted_filter->children.push_back(std::move(plan));
			plan = std::move(lifted_filter);
		}
	} else {
		// Fallback to the normal plan generation path
		// create the blockwise NL join
		op.condition = std::move(conditions[0]);
		for (idx_t i = 1; i < conditions.size(); i++) {
			op.condition = make_uniq<BoundConjunctionExpression>(
			    ExpressionType::CONJUNCTION_AND, std::move(op.condition), std::move(conditions[i]));
		}
		plan = make_uniq<PhysicalBlockwiseNLJoin>(op, std::move(left), std::move(right), std::move(op.condition),
	                                          op.join_type, op.estimated_cardinality);
	}
	return plan;
}

} // namespace duckdb
