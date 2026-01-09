#include "prediction/plan_prediction_util.hpp"
#include "duckdb/planner/expression_iterator.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

namespace prediction {

std::string function_kind_to_string(FunctionKind kind){
    switch(kind){
        case FunctionKind::COMMON:
            return "COMMON";
        case FunctionKind::PREDICTION:
            return "PREDICTION";
        case FunctionKind::PROCESS_PREDICTION:
            return "PROCESS_PREDICTION";
        case FunctionKind::SCHEDULE_PREDICTION:
            return "SCHEDULE_PREDICTION";
        case FunctionKind::THREAD_SCHEDULE_PREDICTION:
            return "THREAD_SCHEDULE_PREDICTION";
    }
    return "NO PREDICTION";
}


bool PredictionFuncChecker::IsPrediction(FunctionKind kind){
    bool res = false;
    switch(kind){
        case FunctionKind::COMMON:
            break;
        case FunctionKind::PREDICTION:
        case FunctionKind::PROCESS_PREDICTION:
        case FunctionKind::SCHEDULE_PREDICTION:
        case FunctionKind::THREAD_SCHEDULE_PREDICTION:
            res = true;
            break;
    }
    return res;
}

bool PredictionFuncChecker::IsProcessPrediction(FunctionKind kind) {
    bool res = false;
    switch(kind){
        case FunctionKind::PROCESS_PREDICTION:
        case FunctionKind::THREAD_SCHEDULE_PREDICTION:
            res = true;
            break;
        default:
            break;
    }
    return res;
} 

bool PredictionFuncChecker::CheckExprs(std::function<bool(idx_t)> constraint, std::function<bool(FunctionKind)> kind_constraint) {
    total_prediction_func_count = 0;
    user_batch_size_map.assign(expressions.size(), 0);
    root_idx_list.clear();

    for(idx_t i = 0; i < expressions.size(); i++) {
        VisitExpression(&expressions[i], i, kind_constraint);
    }
    return constraint(total_prediction_func_count);
}

void PredictionFuncChecker::VisitExpression(unique_ptr<Expression> *expression, idx_t root_idx, std::function<bool(FunctionKind)> kind_constraint) {
	auto &expr = **expression;
    if(expr.GetExpressionClass() == ExpressionClass::BOUND_FUNCTION) {
        auto &func_expr = expr.Cast<BoundFunctionExpression>();
        if(func_expr.function.bridge_info) {
            if(kind_constraint(func_expr.function.bridge_info->kind)) {
                // THREAD_SCHEDULE_PREDICTION has the highest priority
                kind = kind == FunctionKind::THREAD_SCHEDULE_PREDICTION? FunctionKind::THREAD_SCHEDULE_PREDICTION :
                       func_expr.function.bridge_info->kind;
                total_prediction_func_count += 1;
                user_batch_size_map[root_idx] = std::max(idx_t(func_expr.function.bridge_info->batch_size), user_batch_size_map[root_idx]);
                root_idx_list.insert(root_idx);
            }
        }
    }
    VisitExpressionChild(expr, root_idx, kind_constraint);
}


void PredictionFuncChecker::VisitExpressionChild(Expression &expr, idx_t root_idx, std::function<bool(FunctionKind)> kind_constraint) {
    ExpressionIterator::EnumerateChildren(expr, [&](unique_ptr<Expression> &expr) { VisitExpression(&expr, root_idx, kind_constraint); });
}

} // namespace prediction

} // namespace duckdb