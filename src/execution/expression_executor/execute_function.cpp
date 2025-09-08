#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "dbend/c/imlane_dbend.hpp"
#include "prediction/datachunk_converter.hpp"

namespace duckdb {

bool IsPredictionFunc(const BoundFunctionExpression &expr) {
	if(expr.function.bridge_info == nullptr) {
		return false;
	}
	FunctionKind kind = expr.function.bridge_info->kind;
	bool res = false;
    switch(kind){
        case FunctionKind::COMMON:
            break;
        case FunctionKind::PREDICTION:
			res = false;
			break;
        case FunctionKind::PROCESS_PREDICTION:
        case FunctionKind::SCHEDULE_PREDICTION:
            res = true;
            break;
    }
    return res;
}

ExecuteFunctionState::ExecuteFunctionState(const Expression &expr, ExpressionExecutorState &root)
    : ExpressionState(expr, root) {
	exec_ctx = nullptr;
}

ExecuteFunctionState::~ExecuteFunctionState() {
}

unique_ptr<ExpressionState> ExpressionExecutor::InitializeState(const BoundFunctionExpression &expr,
                                                                ExpressionExecutorState &root, idx_t capacity) {
	auto result = make_uniq<ExecuteFunctionState>(expr, root);
	auto &flags = root.executor->sub_expr_eval_flags;
	flags.push_back(0);
	result->eval_flag_idx = flags.size() - 1;

	result->exec_ctx = nullptr;
	if(prediction::core_aware) {
		result->sched_hint = make_uniq<IMLane::DBEnd::SchedHint>();
	} else {
		result->sched_hint = nullptr;
	}
	
	for (auto &child : expr.children) {
		result->AddChild(child.get(), capacity);
	}
	result->Finalize(false, capacity);
	if (expr.function.init_local_state) {
		result->local_state = expr.function.init_local_state(*result, expr, expr.bind_info.get());
	}
	return std::move(result);
}

static void VerifyNullHandling(const BoundFunctionExpression &expr, DataChunk &args, Vector &result) {
#ifdef DEBUG
	if (args.data.empty() || expr.function.null_handling != FunctionNullHandling::DEFAULT_NULL_HANDLING) {
		return;
	}

	// Combine all the argument validity masks into a flat validity mask
	idx_t count = args.size();
	ValidityMask combined_mask(count);
	for (auto &arg : args.data) {
		UnifiedVectorFormat arg_data;
		arg.ToUnifiedFormat(count, arg_data);

		for (idx_t i = 0; i < count; i++) {
			auto idx = arg_data.sel->get_index(i);
			if (!arg_data.validity.RowIsValid(idx)) {
				combined_mask.SetInvalid(i);
			}
		}
	}

	// Default is that if any of the arguments are NULL, the result is also NULL
	UnifiedVectorFormat result_data;
	result.ToUnifiedFormat(count, result_data);
	for (idx_t i = 0; i < count; i++) {
		if (!combined_mask.RowIsValid(i)) {
			auto idx = result_data.sel->get_index(i);
			D_ASSERT(!result_data.validity.RowIsValid(idx));
		}
	}
#endif
}

void ExpressionExecutor::Execute(const BoundFunctionExpression &expr, ExpressionState *state,
                                 const SelectionVector *sel, idx_t count, Vector &result) {
	// only reset args chunk when all args exprs are not evaluated
	// opt: Does nested UDF exist?
	bool args_uneval = true;
	for (idx_t i = 0; i < expr.children.size(); i++) {
		if (state->child_states[i]->IsEvaluated()) {
			args_uneval = false;
			break;
		}
	}

	if (args_uneval) {
		state->intermediate_chunk.Reset();
	}
	auto &arguments = state->intermediate_chunk;
	if (!state->types.empty()) {
		for (idx_t i = 0; i < expr.children.size(); i++) {
			D_ASSERT(state->types[i] == expr.children[i]->return_type);
			Execute(*expr.children[i], state->child_states[i].get(), sel, count, arguments.data[i]);
			if (!state->child_states[i]->IsEvaluated()) {
				return;
			}
#ifdef DEBUG
			if (expr.children[i]->return_type.id() == LogicalTypeId::VARCHAR) {
				arguments.data[i].UTFVerify(count);
			}
#endif
		}
	}
	arguments.SetCardinality(count);
	arguments.Verify();

	D_ASSERT(expr.function.function);
	
	if(pgstate != nullptr) {
		if(IsPredictionFunc(expr) && pgstate->IMLaneOptimize()) {
			auto lane_context = pgstate->lane_context;
			D_ASSERT(lane_context != nullptr);
			auto &state_f = state->Cast<ExecuteFunctionState>();
			IMLane::DBEnd::SchedHint *hint = nullptr;
			if(state_f.sched_hint) {
				hint = state_f.sched_hint.get();
				hint->core_id = prediction::GetCurrentCpu();
			}
			if(pgstate->kind == FunctionKind::PROCESS_PREDICTION) {
				lane_context->ExecuteFunction(expr.function.name, arguments, result, hint);
			} else {
				auto &state_f = state->Cast<ExecuteFunctionState>();
				if(state_f.exec_ctx == nullptr) {
					auto pull_p = lane_context->ExecuteFunctionPush(expr.function.name, arguments, result);
					state_f.exec_ctx = shared_ptr<IMLane::DBEnd::ExecFuncContext<DataChunk, Vector>>(std::move(pull_p));
				} else {
					auto is_ready = state_f.exec_ctx->ExecuteFuncTryPull();
					if(!is_ready) {
						return;
					} else {
						state_f.exec_ctx = nullptr;
					}
				}
			}
		} else {
			expr.function.function(arguments, *state, result);
		}
	} else {
		expr.function.function(arguments, *state, result);
	}

	VerifyNullHandling(expr, arguments, result);
	D_ASSERT(result.GetType() == expr.return_type);
	
	state->SetEvaluated();
}

} // namespace duckdb
