#include "prediction/execution/operator/physical_prediction_filter.hpp"
#include "prediction/execution/exec_prediction_util.hpp"

#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression/bound_conjunction_expression.hpp"
#include "duckdb/parallel/thread_context.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <iostream>

#include "dbend/c/imlane_dbend.hpp"

namespace duckdb {
namespace prediction {

class PredictionFilterState: public PredictionState {
public:
	explicit PredictionFilterState(ExecutionContext &context, 
        PredictionGlobalState* pgstate,
        Expression &expr,
    const vector<LogicalType> &input_types, idx_t prediction_size = INITIAL_PREDICTION_SIZE,
     bool adaptive = false, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : PredictionState(context, input_types, pgstate, expr, adaptive, prediction_size, buffer_capacity),
         executor(context.client, expr, buffer_capacity),
         sel(buffer_capacity), sel_capacity(buffer_capacity) {
            executor.SetPredictionGlobalState(pgstate);
	}

	ExpressionExecutor executor;
	SelectionVector sel;
    idx_t sel_capacity;
    bool initialized=false;
    bool can_cache_chunk=false;

public:
	void Finalize(const PhysicalOperator &op, ExecutionContext &context) override {
		context.thread.profiler.Flush(op, executor, "prediction_filter", 0);
	}
};

struct FilterLaneSlot {
    unique_ptr<ExpressionExecutor> executor;
    unique_ptr<DataChunk> input;
    unique_ptr<DataChunk> output;
    SelectionVector sel;
    std::unique_ptr<IMLane::DBEnd::ExecAsyncFuncContext> task;

    FilterLaneSlot(ClientContext &context, Expression *expression,
     const vector<LogicalType> &input_types, const vector<LogicalType> &output_types, PredictionGlobalState* pgstate):
     sel(STANDARD_VECTOR_SIZE) {
            input = make_uniq<DataChunk>();
            output = make_uniq<DataChunk>();
            executor = make_uniq<ExpressionExecutor>(context, *expression);

            executor->SetPredictionGlobalState(pgstate);

            input->Initialize(context, input_types);
            output->Initialize(context, output_types);
    }
};

class PredictionFilterGlobalState: public PredictionOpGlobalState {
public:
    explicit PredictionFilterGlobalState(const IMLane::DBEnd::IMSettings settings, 
    ClientContext &context, Expression *expression,
     const vector<LogicalType> &input_types, PredictionGlobalState* pgstate): 
        PredictionOpGlobalState(settings) {
        auto num_slots = settings.n_executors;

        for(int i = 0; i < num_slots; i++) {
            sched->Enqueue(i);
            auto pslot = make_uniq<FilterLaneSlot>(context, expression, input_types, input_types, pgstate);
            slots.push_back(std::move(pslot));
        }
    }
    vector<unique_ptr<FilterLaneSlot>> slots;
};


bool PhysicalPredictionFilter::CanCacheType(const LogicalType &type) {
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

PhysicalPredictionFilter::PhysicalPredictionFilter(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
 idx_t estimated_cardinality, idx_t user_defined_size, FunctionKind kind): 
 PhysicalOperator(PhysicalOperatorType::PREDICTION_FILTER, std::move(types), estimated_cardinality), kind(kind) {
	D_ASSERT(select_list.size() > 0);
	if (select_list.size() > 1) {
		// create a big AND out of the expressions
		auto conjunction = make_uniq<BoundConjunctionExpression>(ExpressionType::CONJUNCTION_AND);
		for (auto &expr : select_list) {
			conjunction->children.push_back(std::move(expr));
		}
		expression = std::move(conjunction);
	} else {
		expression = std::move(select_list[0]);
	}

    if(user_defined_size <= 0) {
        this->user_defined_size = INITIAL_PREDICTION_SIZE;
        use_adaptive_size = true; 
    } else {
        this->user_defined_size = user_defined_size;
        use_adaptive_size = false;
    }

    pgstate = new PredictionGlobalState(kind);

    switch(pgstate->kind) {
        case FunctionKind::PREDICTION: {
            exec_func = std::bind(&PhysicalPredictionFilter::BatchingExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5);
            final_exec_func = std::bind(&PhysicalPredictionFilter::BatchingFinalExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4);
            break;
        }
        case FunctionKind::PROCESS_PREDICTION: {
            exec_func = std::bind(&PhysicalPredictionFilter::ProcessExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5);
            final_exec_func = std::bind(&PhysicalPredictionFilter::ProcessFinalExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4);
            break;
        }
        case FunctionKind::SCHEDULE_PREDICTION: {
            exec_func = std::bind(&PhysicalPredictionFilter::ProcessSchedExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5);
            final_exec_func = std::bind(&PhysicalPredictionFilter::ProcessSchedFinalExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4);
            break;
        }
        case FunctionKind::THREAD_SCHEDULE_PREDICTION: {
            exec_func = std::bind(&PhysicalPredictionFilter::ProcessSchedPoolExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4,
                std::placeholders::_5);
            final_exec_func = std::bind(&PhysicalPredictionFilter::ProcessSchedFinalPoolExec, this, 
                std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4);
            break;
        }
        default: {
            throw InternalException("Unsupported Prediction Function Kind.");
        }
    }

    caching_supported = true;
    for (auto &col_type : types) {
        if (!CanCacheType(col_type)) {
            caching_supported = false;
            break;
        }
    }
}

OperatorResultType PhysicalPredictionFilter::BatchingExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state) const {
        auto &state_p = state.Cast<PredictionFilterState>();

        auto &controller = state_p.controller;
        auto &sel = state_p.sel;
        auto &padded = state_p.padded;
        auto &output_left = state_p.output_left;
        auto &base_offset = state_p.base_offset;
        idx_t &batch_size = state_p.prediction_size;
    
        if (!state_p.initialized) {
            state_p.initialized = true;
            state_p.can_cache_chunk = caching_supported && PhysicalOperator::OperatorCachingAllowed(context);
        }
        if(!state_p.can_cache_chunk){
            idx_t result_count = state_p.executor.SelectExpression(input, state_p.sel);
            if (result_count == input.size()) {
                // nothing was filtered: skip adding any selection vectors
                chunk.Reference(input);
            } else {
                chunk.Slice(input, state_p.sel, result_count);
            }
            return OperatorResultType::NEED_MORE_INPUT;
        }
    
        auto ret = OperatorResultType::HAVE_MORE_OUTPUT;
    
        // batch adapting
        if (output_left) {
            if (output_left <= STANDARD_VECTOR_SIZE) {
                controller->BatchAdapting(controller->CurrentBatch(), sel, chunk, base_offset, output_left);
                output_left = 0;
                base_offset = 0;
            } else {
                controller->BatchAdapting(controller->CurrentBatch(), sel, chunk, base_offset);
                output_left -= STANDARD_VECTOR_SIZE;
                base_offset += STANDARD_VECTOR_SIZE;
            }
    
            return ret;
        }
    
        switch (controller->GetState()) {
            case BatchControllerState::SLICING: {
                batch_size = state_p.tuner.GetBatchSize();
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
                batch_size = state_p.tuner.GetBatchSize();
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
                batch_size = state_p.tuner.GetBatchSize();
    
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

OperatorResultType PhysicalPredictionFilter::ProcessExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state) const {
    auto &state_p = state.Cast<PredictionFilterState>();
    
    idx_t result_count = state_p.executor.SelectExpression(input, state_p.sel);
    if (result_count == input.size()) {
        // nothing was filtered: skip adding any selection vectors
        chunk.Reference(input);
    } else {
        chunk.Slice(input, state_p.sel, result_count);
    }
    return OperatorResultType::NEED_MORE_INPUT;
}

OperatorResultType PhysicalPredictionFilter::ProcessSchedExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state) const {
    return OperatorResultType::FINISHED;
}

OperatorResultType PhysicalPredictionFilter::ProcessSchedPoolExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const {
        auto &state = state_p.Cast<PredictionFilterState>();
        auto &gstate_c = gstate.Cast<PredictionFilterGlobalState>();
        auto ret = OperatorResultType::HAVE_MORE_OUTPUT;
    
        switch(state.sched_state) {
            case SchedState::SCHEDULE: {
                int slot_id; 
                bool sched_ok = gstate_c.sched->TryDequeue(slot_id);
                if(sched_ok) {
                    auto slot = gstate_c.slots[slot_id].get();
                    input.Copy(*slot->input);
                    // slot->executor->Execute(*slot->input, *slot->output);
                    slot->task = std::move(state.pgstate->lane_context->ExecuteAsyncFuncPush([slot]() {
                        idx_t result_count = slot->executor->SelectExpression(*slot->input, slot->sel);
                        if (result_count == slot->input->size()) {
                            // nothing was filtered: skip adding any selection vectors
                            slot->output->Reference(*slot->input);
                        } else {
                            slot->output->Slice(*slot->input, slot->sel, result_count);
                        }
                    }));
                    if(slot->task == nullptr) {
                        throw std::runtime_error("Failed to create async task");
                    }
                    state.sched_slot_ids.push_back(slot_id);
                    ret = OperatorResultType::NEED_MORE_INPUT;
                } else {
                    state.sched_state = SchedState::OUTPUT;
                }
                break;
            }
            case SchedState::OUTPUT: {
                bool has_ready = false;
                while(state.sched_slot_ids.size()) {
                    for(auto it = state.sched_slot_ids.begin(); it!=state.sched_slot_ids.end(); ++it) {
                        auto slot = gstate_c.slots[*it].get();
                        // bool is_ready = slot->task->ExecuteAsyncFuncTryPull();
                        slot->task->ExecuteAsyncFuncPull();
                        bool is_ready = true;
                        if(is_ready){
                            slot->output->Copy(chunk);
                            state.sched_slot_ids.erase(it);
                            gstate_c.sched->Enqueue(*it);
                            has_ready = true;
                            break;
                        }
                    }
                    if(has_ready) {
                        break;
                    }
                }
    
                state.sched_state = SchedState::SCHEDULE;
            }
        }
    
        return ret;
}

unique_ptr<OperatorState> PhysicalPredictionFilter::GetOperatorState(ExecutionContext &context) const {
	return make_uniq<PredictionFilterState>(context, pgstate,  *expression, children[0]->GetTypes(), user_defined_size, use_adaptive_size);
}

unique_ptr<GlobalOperatorState> PhysicalPredictionFilter::GetGlobalOperatorState(ClientContext &context) const {
    if(pgstate->kind == FunctionKind::SCHEDULE_PREDICTION || pgstate->kind == FunctionKind::THREAD_SCHEDULE_PREDICTION) {
        return make_uniq<PredictionFilterGlobalState>(pgstate->lane_context->GetSettings(),
         context, expression.get(), children[0]->GetTypes(), pgstate);
    }
    return make_uniq<GlobalOperatorState>();
}

template<typename RET_TYPE>
RET_TYPE PhysicalPredictionFilter::NextEvalAdapt(OperatorState &state, idx_t batch_size, DataChunk &chunk,
 RET_TYPE ret_adapt, RET_TYPE no_adapt) const {
    auto &local = state.Cast<PredictionFilterState>();
    auto &controller = local.controller;
    auto &sel = local.sel;
    auto &sel_capacity = local.sel_capacity;
    auto &tuner = local.tuner;

    RET_TYPE ret;

    auto &batch = controller->NextBatch(batch_size);
    controller->ExternalFilterReset(sel, sel_capacity, local.executor);
    tuner.StartProfile();
    idx_t result_count = local.executor.SelectExpression(batch, sel);
    tuner.EndProfile();
    if (result_count > STANDARD_VECTOR_SIZE) {
        controller->BatchAdapting(batch, sel, chunk, local.base_offset);

        local.output_left = result_count - STANDARD_VECTOR_SIZE;
        local.base_offset += STANDARD_VECTOR_SIZE;

        ret = ret_adapt;
    } else {
        if (batch_size <= STANDARD_VECTOR_SIZE && result_count == batch_size) {
            // nothing was filtered: skip adding any selection vectors
            chunk.Reference(batch);
        } else {
            chunk.Slice(batch, sel, result_count);
        }
        
        ret = no_adapt;
    }

    return ret;
}

OperatorResultType PhysicalPredictionFilter::Execute(ExecutionContext &context, DataChunk &input,
 DataChunk &chunk, GlobalOperatorState &gstate, OperatorState &state) const {
    return exec_func(context, input, chunk, gstate, state);
}

OperatorFinalizeResultType  PhysicalPredictionFilter::BatchingFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
    OperatorState &state) const{
        auto &local = state.Cast<PredictionFilterState>();
        auto &controller = local.controller;
        auto &sel = local.sel;
    
        auto &output_left = local.output_left;
        auto &base_offset = local.base_offset;
    
        idx_t batch_size = local.prediction_size;
    
        auto ret = OperatorFinalizeResultType::FINISHED;
    
        // batch adapting for the rest of output chunk
        if (output_left) {
            if (output_left <= STANDARD_VECTOR_SIZE) {
                controller->BatchAdapting(controller->CurrentBatch(), sel, chunk, base_offset, output_left);
                output_left = 0;
                base_offset = 0;
            } else {
                controller->BatchAdapting(controller->CurrentBatch(), sel, chunk, base_offset);
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

OperatorFinalizeResultType  PhysicalPredictionFilter::ProcessFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
OperatorState &state) const{
    return OperatorFinalizeResultType::FINISHED;
}

OperatorFinalizeResultType  PhysicalPredictionFilter::ProcessSchedFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
    OperatorState &state) const {
    return OperatorFinalizeResultType::FINISHED;
}
OperatorFinalizeResultType  PhysicalPredictionFilter::ProcessSchedFinalPoolExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
    OperatorState &state) const {
        auto &local = state.Cast<PredictionFilterState>();
        auto &gstate_c = gstate.Cast<PredictionFilterGlobalState>();
    
        auto ret = OperatorFinalizeResultType::HAVE_MORE_OUTPUT;
        bool has_ready = false;
    
        while(local.sched_slot_ids.size()) {
            for(auto it = local.sched_slot_ids.begin(); it!=local.sched_slot_ids.end(); ++it) {
                auto &slot = *gstate_c.slots[*it];
                // bool is_ready = slot.task->ExecuteAsyncFuncTryPull();
                slot.task->ExecuteAsyncFuncPull();
                bool is_ready = true;
                if(is_ready){
                    slot.output->Copy(chunk);
                    local.sched_slot_ids.erase(it);
                    gstate_c.sched->Enqueue(*it);
                    has_ready = true;
                    break;
                }
            }
            if(has_ready) {
                return ret;
            }
        }
    
        if(local.sched_slot_ids.size() == 0) {
            ret = OperatorFinalizeResultType::FINISHED;
        }
        return ret;
}

OperatorFinalizeResultType PhysicalPredictionFilter::FinalExecute(ExecutionContext &context,
 DataChunk &chunk, GlobalOperatorState &gstate, OperatorState &state) const {
    return final_exec_func(context, chunk, gstate, state);
 }

string PhysicalPredictionFilter::ParamsToString() const {
    auto result = expression->GetName();
    result += "\n[INFOSEPARATOR]\n";
    result += StringUtil::Format("EC: %llu", estimated_cardinality);
    if(!use_adaptive_size){
        result += StringUtil::Format("\nprediction size: %llu\n", user_defined_size);
    } else {
        result += "\nprediction size: adaptive\n";
    }
    result += "prediction kind: " + function_kind_to_string(kind) + "\n";
    return result;
}

} // namespace prediction
    

} // namespace duckdb
