#pragma once

#include "duckdb/execution/physical_operator_states.hpp"
#include "duckdb/common/unordered_set.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/common/vector.hpp"

#include "imbridge/execution/batch_controller.hpp"

namespace duckdb {

enum class FunctionKind: u_int8_t {COMMON = 0, BATCH_PREDICTION=1, ASYNC_PREDICTION=2, SCHEDULE_PREDICTION=3, ASYNC_BATCH_PREDICTION=4};

#define DEFAULT_PREDICTION_BATCH_SIZE 2048U

struct IMBridgeExtraInfo
{
	FunctionKind kind = FunctionKind::COMMON;
	u_int32_t batch_size = DEFAULT_PREDICTION_BATCH_SIZE;

	IMBridgeExtraInfo(FunctionKind kind, u_int32_t batch_size): kind(kind), batch_size(batch_size) {};
};

namespace imbridge {

class PredictionFuncChecker {

public:
	explicit PredictionFuncChecker(vector<unique_ptr<Expression>>& expressions): expressions(expressions) {
        user_batch_size_map.resize(expressions.size());
    }

    // Check wheather multiple expressions satisfy the optimization constraints,
    // also collect the prediction function info.
    bool CheckExprs(std::function<bool(idx_t)> constraint);

    vector<idx_t> user_batch_size_map;
    set<idx_t> root_idx_list;
    idx_t total_prediction_func_count;
    FunctionKind kind;

private:

    vector<unique_ptr<Expression>>& expressions;
	void VisitExpression(unique_ptr<Expression> *expression, idx_t root_idx);

	void VisitExpressionChild(Expression &expression, idx_t root_idx);

};

} // namespace imbridge

namespace imbridge {

#define DEFAULT_RESERVED_CAPACITY STANDARD_VECTOR_SIZE*4
#define INITIAL_PREDICTION_SIZE DEFAULT_PREDICTION_BATCH_SIZE

class PredictionState : public OperatorState {
public:
	explicit PredictionState(ExecutionContext &context,const vector<LogicalType> &input_types,
	idx_t prediction_size = INITIAL_PREDICTION_SIZE, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : prediction_size(prediction_size), padded(0), output_left(0), base_offset(0) {
            controller = make_uniq<BatchController>();
            controller->Initialize(Allocator::Get(context.client), input_types,  buffer_capacity);
	}

    unique_ptr<BatchController> controller;
    idx_t prediction_size;

	idx_t padded;

    // slicing range for batch adapting
    idx_t output_left;
    idx_t base_offset;
};

} // namespace imbridge

} // namespace duckdb