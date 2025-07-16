#pragma once

#include "duckdb/execution/physical_operator_states.hpp"
#include "duckdb/common/unordered_set.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/common/vector.hpp"

#include "imbridge/execution/batch_controller.hpp"
#include "imbridge/execution/adaptive_batch_tuner.hpp"

namespace duckdb {

enum class FunctionKind: u_int8_t {COMMON = 0, PREDICTION=1, PROCESS_PREDICTION=2, SCHEDULE_PREDICTION=3};

#define DEFAULT_PREDICTION_BATCH_SIZE 2048U

struct IMBridgeExtraInfo
{
	FunctionKind kind = FunctionKind::COMMON;
	u_int32_t batch_size = DEFAULT_PREDICTION_BATCH_SIZE;

	IMBridgeExtraInfo(FunctionKind kind, u_int32_t batch_size): kind(kind), batch_size(batch_size) {};
};

namespace imbridge {

std::string function_kind_to_string(FunctionKind kind);

class PredictionFuncChecker {

public:
	explicit PredictionFuncChecker(vector<unique_ptr<Expression>>& expressions): expressions(expressions) {
        user_batch_size_map.resize(expressions.size());
    }

    // Check wheather multiple expressions satisfy the optimization constraints,
    // also collect the prediction function info.
    bool CheckExprs(std::function<bool(idx_t)> constraint);

    bool IsPrediction(FunctionKind kind);

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

    // for multiple expressions
	explicit PredictionState(ExecutionContext &context,const vector<LogicalType> &input_types,
    const vector<unique_ptr<Expression>> &expressions, bool adaptive,
	idx_t prediction_size = INITIAL_PREDICTION_SIZE, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : tuner(prediction_size, adaptive), prediction_size(prediction_size),
         padded(0), output_left(0), base_offset(0) {
            controller = make_uniq<BatchController>();
            controller->Initialize(Allocator::Get(context.client), input_types,  buffer_capacity);
	}

    // for single expression
    explicit PredictionState(ExecutionContext &context,const vector<LogicalType> &input_types,
    Expression &expr, bool adaptive,
	idx_t prediction_size = INITIAL_PREDICTION_SIZE, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : tuner(prediction_size, adaptive), prediction_size(prediction_size),
         padded(0), output_left(0), base_offset(0) {
            controller = make_uniq<BatchController>();
            controller->Initialize(Allocator::Get(context.client), input_types,  buffer_capacity);
	}

    AdaptiveBatchTuner tuner;
    unique_ptr<BatchController> controller;
    idx_t prediction_size;

	idx_t padded;

    // slicing range for batch adapting
    idx_t output_left;
    idx_t base_offset;

};

} // namespace imbridge

} // namespace duckdb