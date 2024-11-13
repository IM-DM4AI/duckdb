#include "imbridge/execution/operator/physical_prediction_projection.hpp"
#include "duckdb/parallel/thread_context.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "imbridge/execution/adaptive_batch_tuner.hpp"

namespace duckdb {

namespace imbridge {

#define NEXT_EXE_ADAPT(CTX, STATE, X, SIZE, Y, Z, IF_RET_TYPE, ELSE_RET_TYPE, RET) \
auto &batch = X->NextBatch(SIZE); \
X->ExternalProjectionReset(CTX, *Y, STATE.executor); \
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

class PredictionProjectionState : public PredictionState {
public:
	explicit PredictionProjectionState(ExecutionContext &context, const vector<unique_ptr<Expression>> &expressions,
    const vector<LogicalType> &input_types, idx_t prediction_size = INITIAL_PREDICTION_SIZE, bool adaptive = false, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : PredictionState(context, input_types, prediction_size, buffer_capacity),
         executor(context.client, expressions, buffer_capacity), tuner(prediction_size, adaptive){
			output_buffer = make_uniq<DataChunk>();
            vector<LogicalType> output_types;

            for(auto & expr: expressions) {
                output_types.push_back(expr->return_type);
            }
            output_buffer->Initialize(Allocator::Get(context.client), output_types,  buffer_capacity);
		}

	ExpressionExecutor executor;
	unique_ptr<DataChunk> output_buffer;
    AdaptiveBatchTuner tuner;

public:
	void Finalize(const PhysicalOperator &op, ExecutionContext &context) override {
		context.thread.profiler.Flush(op, executor, "prediction_projection", 0);
	}
};

PhysicalPredictionProjection::PhysicalPredictionProjection(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
                                       idx_t estimated_cardinality, idx_t user_defined_size)
    : PhysicalOperator(PhysicalOperatorType::PREDICTION_PROJECTION, std::move(types), estimated_cardinality),
      select_list(std::move(select_list)) {
        if(user_defined_size <= 0) {
            this->user_defined_size = INITIAL_PREDICTION_SIZE;
            use_adaptive_size = true; 
        } else {
            this->user_defined_size = user_defined_size;
            use_adaptive_size = false;
        }
}

OperatorResultType PhysicalPredictionProjection::Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
                                               GlobalOperatorState &gstate, OperatorState &state_p) const {
	auto &state = state_p.Cast<PredictionProjectionState>();
    auto &controller = state.controller;
    auto &out_buf = state.output_buffer;
    auto &padded = state.padded;
    auto &output_left = state.output_left;
    auto &base_offset = state.base_offset;
    idx_t &batch_size = state.prediction_size;

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
            NEXT_EXE_ADAPT(context.client, state, controller, batch_size, out_buf, chunk,
             OperatorResultType::HAVE_MORE_OUTPUT, OperatorResultType::HAVE_MORE_OUTPUT, ret);
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
            controller->PushChunk(context.client, input, padded, input.size());
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
            controller->PushChunk(context.client, input);
            controller->SetState(BatchControllerState::BUFFERRING);
            ret = OperatorResultType::NEED_MORE_INPUT;
        } else {
            padded = batch_size - controller->GetSize();
            controller->PushChunk(context.client, input, 0, padded);

            NEXT_EXE_ADAPT(context.client, state, controller, batch_size, out_buf, chunk, 
            OperatorResultType::HAVE_MORE_OUTPUT, OperatorResultType::HAVE_MORE_OUTPUT, ret);

            controller->SetState(BatchControllerState::EMPTY);
        }  
        break;
    }
    
    default:
        throw InternalException("ChunkBuffer State Unsupported");
    }

    return ret;
}

unique_ptr<OperatorState> PhysicalPredictionProjection::GetOperatorState(ExecutionContext &context) const {
    D_ASSERT(children.size() == 1);
    return make_uniq<PredictionProjectionState>(context, select_list, children[0]->GetTypes(), user_defined_size, use_adaptive_size);
}

string PhysicalPredictionProjection::ParamsToString() const {
	string extra_info;
	for (auto &expr : select_list) {
		extra_info += expr->GetName() + "\n";
	}
    extra_info += use_adaptive_size? "adaptive": "prediction_size:" + std::to_string(user_defined_size) + "\n";
	return extra_info;
}

OperatorFinalizeResultType PhysicalPredictionProjection::FinalExecute(ExecutionContext &context,
 DataChunk &chunk, GlobalOperatorState &gstate, OperatorState &state) const {
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
        NEXT_EXE_ADAPT(context.client, local, controller, batch_size, out_buf, chunk, 
        OperatorFinalizeResultType::HAVE_MORE_OUTPUT, OperatorFinalizeResultType::HAVE_MORE_OUTPUT, ret);
    } else {
        if (controller->GetSize() > 0) {
            NEXT_EXE_ADAPT(context.client, local, controller, controller->GetSize(), out_buf, chunk, 
            OperatorFinalizeResultType::HAVE_MORE_OUTPUT, OperatorFinalizeResultType::FINISHED, ret);
        } 
    }

    return ret;
}

} // namespace imbridge

} // namespace duckdb