#include "prediction/execution/operator/physical_prediction_projection.hpp"
#include "prediction/execution/exec_prediction_util.hpp"
#include "duckdb/parallel/thread_context.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include <fstream>
#include <string>
#include <iostream>

#include "dbend/c/imlane_dbend.hpp"

namespace duckdb {

namespace prediction {

// remove this opaque macro
/* #define NEXT_EXE_ADAPT(STATE, X, SIZE, Y, Z, IF_RET_TYPE, ELSE_RET_TYPE, RET) \
auto &batch = X->NextBatch(SIZE); \
X->ExternalProjectionReset(*Y, STATE.executor); \
STATE.tuner.StartProfile(); \
STATE.executor.Execute(batch, *Y); \
STATE.tuner.EndProfile(); \
if (Y->size() > STANDARD_VECTOR_SIZE) { \
    X->BatchAdapting(*Y, Z, STATE.base_offset); \
    STATE.output_left = Y->size() - STANDARD_VECTOR_SIZE; \
    STATE.base_offset += STANDARD_VECTOR_SIZE; \
    RET = IF_RET_TYPE;\
} else { \
    Z.Reference(*Y); \
    RET = ELSE_RET_TYPE;\
}\
*/

class PredictionProjectionState : public PredictionState {
public:
	explicit PredictionProjectionState(ExecutionContext &context,
        PredictionGlobalState* pgstate,
        const vector<unique_ptr<Expression>> &expressions,
    const vector<LogicalType> &input_types, idx_t prediction_size = INITIAL_PREDICTION_SIZE, bool adaptive = false, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : PredictionState(context, input_types, pgstate, expressions, adaptive, prediction_size, buffer_capacity),
         executor(context.client, expressions, buffer_capacity) {
			output_buffer = make_uniq<DataChunk>();
            vector<LogicalType> output_types;

            for(auto & expr: expressions) {
                output_types.push_back(expr->return_type);
            }
            output_buffer->Initialize(Allocator::Get(context.client), output_types,  buffer_capacity);

            executor.SetPredictionGlobalState(pgstate);
		}
    bool initialized=false;
    bool can_cache_chunk=false;
	ExpressionExecutor executor;
	unique_ptr<DataChunk> output_buffer;

public:
	void Finalize(const PhysicalOperator &op, ExecutionContext &context) override {
		context.thread.profiler.Flush(op, executor, "prediction_projection", 0);
	}
};

bool PhysicalPredictionProjection::CanCacheType(const LogicalType &type) {
	switch (type.id()) {
	case LogicalTypeId::LIST:
	case LogicalTypeId::MAP:
	case LogicalTypeId::ARRAY:
		return false;
	case LogicalTypeId::STRUCT: {
		auto &entries = StructType::GetChildTypes(type);
		for (auto &entry : entries) {
			if (!CanCacheType(entry.second)) {
				return false;
			}
		}
		return true;
	}
	default:
		return true;
	}
}

PhysicalPredictionProjection::PhysicalPredictionProjection(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
                                       idx_t estimated_cardinality, idx_t user_defined_size, FunctionKind kind)
    : PhysicalOperator(PhysicalOperatorType::PREDICTION_PROJECTION, std::move(types), estimated_cardinality),
      select_list(std::move(select_list)),kind(kind) {

        pgstate = new PredictionGlobalState(kind);

        if(user_defined_size <= 0) {
            this->user_defined_size = INITIAL_PREDICTION_SIZE;
            use_adaptive_size = true; 
        } else {
            this->user_defined_size = user_defined_size;
            use_adaptive_size = false;
        }

        if(pgstate->IMLaneOptimize()) {
            exec_func = std::bind(&PhysicalPredictionProjection::ProcessExec, this, 
            std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4,
            std::placeholders::_5);
        } else {
            exec_func = std::bind(&PhysicalPredictionProjection::BatchingExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5);
        }

        caching_supported = true;
        for (auto &col_type : types) {
            if (!CanCacheType(col_type)) {
                caching_supported = false;
                break;
            }
        }
}

template<typename RET_TYPE>
RET_TYPE PhysicalPredictionProjection::NextEvalAdapt(OperatorState &state, idx_t batch_size, DataChunk &chunk,
 RET_TYPE ret_adapt, RET_TYPE no_adapt) const {
    auto &local = state.Cast<PredictionProjectionState>();
    auto &controller = local.controller;
    auto &out_buf = local.output_buffer;
    auto &tuner = local.tuner;

    RET_TYPE ret;

    auto &batch = controller->NextBatch(batch_size);
    controller->ExternalProjectionReset(*out_buf, local.executor);
    tuner.StartProfile();
    local.executor.Execute(batch, *out_buf);
    tuner.EndProfile();

    if (out_buf->size() > STANDARD_VECTOR_SIZE) {
        controller->BatchAdapting(*out_buf, chunk, local.base_offset);
        local.output_left = out_buf->size() - STANDARD_VECTOR_SIZE;
        local.base_offset += STANDARD_VECTOR_SIZE;

        ret = ret_adapt;
    } else {
        chunk.Reference(*out_buf);
        ret = no_adapt;
    }

    return ret;
}

OperatorResultType PhysicalPredictionProjection::BatchingExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const {
        auto &state = state_p.Cast<PredictionProjectionState>();

        auto &controller = state.controller;
        auto &out_buf = state.output_buffer;
        auto &padded = state.padded;
        auto &output_left = state.output_left;
        auto &base_offset = state.base_offset;
        idx_t &batch_size = state.prediction_size;
        if (!state.initialized) {
            state.initialized = true;
            state.can_cache_chunk = caching_supported && PhysicalOperator::OperatorCachingAllowed(context);
        }
        if(!state.can_cache_chunk){
            state.executor.Execute(input, chunk);
            return OperatorResultType::NEED_MORE_INPUT;
        }

        auto ret = OperatorResultType::HAVE_MORE_OUTPUT;

        // batch adapting
        if (output_left) {
            if (output_left <= STANDARD_VECTOR_SIZE) {
                controller->BatchAdapting(*out_buf, chunk, base_offset, output_left);
                output_left = 0;
                base_offset = 0;
            } else {
                controller->BatchAdapting(*out_buf, chunk, base_offset);
                output_left -= STANDARD_VECTOR_SIZE;
                base_offset += STANDARD_VECTOR_SIZE;
            }

            return ret;
        }

        switch (controller->GetState()) {
        case BatchControllerState::SLICING: {
            batch_size = state.tuner.GetBatchSize();
            if (controller->HasNext(batch_size)) {
                ret = NextEvalAdapt(state, batch_size, chunk,
                OperatorResultType::HAVE_MORE_OUTPUT, OperatorResultType::HAVE_MORE_OUTPUT);
            } else {
                // check wheather the buffer should be reset
                if (controller->GetSize() == 0) {
                    // the buffer state is reset to EMPTY
                    controller->ResetBuffer();
                } else {
                    controller->SetState(BatchControllerState::BUFFERRING);
                }
                ret = OperatorResultType::NEED_MORE_INPUT;
            }
            break;
        }
        case BatchControllerState::EMPTY: {
            batch_size = state.tuner.GetBatchSize();
            controller->ResetBuffer();
            idx_t remained = input.size() - padded;
            ret = OperatorResultType::NEED_MORE_INPUT;

            if (remained > 0) {
                controller->PushChunk(input, padded, input.size());
                if (remained < batch_size) {
                    controller->SetState(BatchControllerState::BUFFERRING); 
                } else {
                    // opt: perform slicing directly
                    controller->SetState(BatchControllerState::SLICING);
                    ret = OperatorResultType::HAVE_MORE_OUTPUT;
                }
            }
            padded = 0;
            break;
        }
        case BatchControllerState::BUFFERRING: {
            batch_size = state.tuner.GetBatchSize();

            if (controller->GetSize() + input.size() < batch_size) {
                controller->PushChunk(input);
                controller->SetState(BatchControllerState::BUFFERRING);
                ret = OperatorResultType::NEED_MORE_INPUT;
            } else {
                padded = batch_size - controller->GetSize();
                controller->PushChunk(input, 0, padded);
                ret = NextEvalAdapt(state, batch_size, chunk,
                OperatorResultType::HAVE_MORE_OUTPUT, OperatorResultType::HAVE_MORE_OUTPUT);
                controller->SetState(BatchControllerState::EMPTY);
            }  
            break;
        }
        
        default:
            throw InternalException("BatchController State Unsupported");
        }

        return ret;
}

OperatorResultType PhysicalPredictionProjection::ProcessExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const {
        auto &state = state_p.Cast<PredictionProjectionState>();
        state.executor.Execute(input, chunk);
        return OperatorResultType::NEED_MORE_INPUT;
}

OperatorResultType PhysicalPredictionProjection::ProcessSchedExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const {
    return OperatorResultType::FINISHED;
}

OperatorResultType PhysicalPredictionProjection::Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
                                               GlobalOperatorState &gstate, OperatorState &state_p) const {
    return exec_func(context, input, chunk, gstate, state_p);
}

unique_ptr<OperatorState> PhysicalPredictionProjection::GetOperatorState(ExecutionContext &context) const {
    D_ASSERT(children.size() == 1);
    return make_uniq<PredictionProjectionState>(context, pgstate, select_list, children[0]->GetTypes(), user_defined_size, use_adaptive_size);
}

unique_ptr<GlobalOperatorState> PhysicalPredictionProjection::GetGlobalOperatorState(ClientContext &context) const {
    return make_uniq<GlobalOperatorState>();
}

string PhysicalPredictionProjection::ParamsToString() const {
	string extra_info;
	for (auto &expr : select_list) {
		extra_info += expr->GetName() + "\n";
	}
    extra_info += use_adaptive_size? "adaptive": "prediction_size:" + std::to_string(user_defined_size) + "\n";
    extra_info += "prediction kind: " + function_kind_to_string(kind) + "\n";
	return extra_info;
}

OperatorFinalizeResultType PhysicalPredictionProjection::FinalExecute(ExecutionContext &context,
 DataChunk &chunk, GlobalOperatorState &gstate, OperatorState &state) const {

    if(pgstate->IMLaneOptimize()) {
        return OperatorFinalizeResultType::FINISHED;
    }
    auto &local = state.Cast<PredictionProjectionState>();
    auto &controller = local.controller;
    auto &out_buf = local.output_buffer;

    auto &output_left = local.output_left;
    auto &base_offset = local.base_offset;

    idx_t batch_size = local.prediction_size;

    auto ret = OperatorFinalizeResultType::FINISHED;

    // batch adapting for the rest of output chunk
    if (output_left) {
        if (output_left <= STANDARD_VECTOR_SIZE) {
            controller->BatchAdapting(*out_buf, chunk, base_offset, output_left);
            output_left = 0;
            base_offset = 0;
        } else {
            controller->BatchAdapting(*out_buf, chunk, base_offset);
            output_left -= STANDARD_VECTOR_SIZE;
            base_offset += STANDARD_VECTOR_SIZE;
        }

        ret = OperatorFinalizeResultType::HAVE_MORE_OUTPUT;

        return ret;
    }

    if (controller->HasNext(batch_size)) {
        ret = NextEvalAdapt(local, batch_size, chunk,
         OperatorFinalizeResultType::HAVE_MORE_OUTPUT, OperatorFinalizeResultType::HAVE_MORE_OUTPUT);
    } else {
        if (controller->GetSize() > 0) {
            ret = NextEvalAdapt(local, controller->GetSize(), chunk,
            OperatorFinalizeResultType::HAVE_MORE_OUTPUT, OperatorFinalizeResultType::FINISHED);
        } 
    }

    return ret;
}

} // namespace prediction

} // namespace duckdb