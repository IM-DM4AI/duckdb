#pragma once

#include "prediction/params.hpp"
#include "duckdb/common/unordered_set.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

enum class FunctionKind: u_int8_t {COMMON = 0, PREDICTION=1, PROCESS_PREDICTION=2, SCHEDULE_PREDICTION=3};

struct IMBridgeExtraInfo
{
	FunctionKind kind = FunctionKind::COMMON;
	u_int32_t batch_size = DEFAULT_PREDICTION_BATCH_SIZE;

	IMBridgeExtraInfo(FunctionKind kind, u_int32_t batch_size): kind(kind), batch_size(batch_size) {};
};

namespace prediction {

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

} // namespace prediction

} // namespace duckdb