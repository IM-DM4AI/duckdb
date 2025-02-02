#include "imbridge/execution/operator/physical_prediction_projection.hpp"

#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/parallel/thread_context.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "imbridge/execution/adaptive_batch_tuner.hpp"

#include <future>
namespace duckdb {

namespace imbridge {

/*
#define NEXT_EXE_ADAPT(CTX, STATE, X, SIZE, Y, Z, IF_RET_TYPE, ELSE_RET_TYPE, RET)                                     \
    auto &batch = X->NextBatch(SIZE);                                                                                  \
    X->ExternalProjectionReset(CTX, *Y, STATE.executor);                                                               \
    STATE.tuner.StartProfile();                                                                                        \
    STATE.executor.Execute(batch, *Y);                                                                                 \
    STATE.tuner.EndProfile();                                                                                          \
    if (Y->size() > STANDARD_VECTOR_SIZE) {                                                                            \
        X->BatchAdapting(*Y, Z, STATE.base_offset);                                                                    \
        STATE.output_left = Y->size() - STANDARD_VECTOR_SIZE;                                                          \
        STATE.base_offset += STANDARD_VECTOR_SIZE;                                                                     \
        RET = IF_RET_TYPE;                                                                                             \
    } else {                                                                                                           \
        Z.Reference(*Y);                                                                                               \
        RET = ELSE_RET_TYPE;                                                                                           \
    }
*/

class PredictionProjectionState : public PredictionState {
public:
	explicit PredictionProjectionState(ExecutionContext &context, const vector<unique_ptr<Expression>> &expressions,
	                                   const vector<LogicalType> &input_types,
	                                   idx_t prediction_size = INITIAL_PREDICTION_SIZE, bool adaptive = false,
	                                   idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
	    : PredictionState(context, input_types, prediction_size, buffer_capacity),
	      executor(context.client, expressions, buffer_capacity), tuner(prediction_size, adaptive) {
		output_buffer = make_uniq<DataChunk>();
		vector<LogicalType> output_types;

		for (auto &expr : expressions) {
			output_types.push_back(expr->return_type);
		}
		output_buffer->Initialize(Allocator::Get(context.client), output_types, buffer_capacity);
	}

	ExpressionExecutor executor;
	unique_ptr<DataChunk> output_buffer;
	AdaptiveBatchTuner tuner;
	std::vector<std::future<unique_ptr<DataChunk>>> res_collect;

public:
	void Finalize(const PhysicalOperator &op, ExecutionContext &context) override {
		context.thread.profiler.Flush(op, executor, "prediction_projection", 0);
	}
};

PhysicalPredictionProjection::PhysicalPredictionProjection(vector<LogicalType> types,
                                                           vector<unique_ptr<Expression>> select_list,
                                                           idx_t estimated_cardinality, idx_t user_defined_size,
                                                           FunctionKind kind)
    : PhysicalOperator(PhysicalOperatorType::PREDICTION_PROJECTION, std::move(types), estimated_cardinality),
      select_list(std::move(select_list)), kind(kind) {
	if (user_defined_size <= 0) {
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
	if (kind == FunctionKind::BATCH_PREDICTION || kind == FunctionKind::ASYNC_BATCH_PREDICTION) {
		auto &out_buf = state.output_buffer;
		auto &padded = state.padded;
		auto &output_left = state.output_left;
		auto &base_offset = state.base_offset;
		idx_t &batch_size = state.prediction_size;
		bool is_async = kind == FunctionKind::ASYNC_BATCH_PREDICTION;
		auto ret = OperatorResultType::HAVE_MORE_OUTPUT;

		// batch adapting
		if (output_left) {
			if (output_left <= STANDARD_VECTOR_SIZE) {
				controller->BatchAdapting(*out_buf, chunk, base_offset, output_left);
				output_left = 0;
				base_offset = 0;
				out_buf->Reset();
			} else {
				controller->BatchAdapting(*out_buf, chunk, base_offset);
				output_left -= STANDARD_VECTOR_SIZE;
				base_offset += STANDARD_VECTOR_SIZE;
			}
			return ret;
		}

		if (is_async) {
			auto &res_collect = state.res_collect;
			for (auto it = res_collect.begin(); it != res_collect.end(); ++it) {
				if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
					try {
						unique_ptr<DataChunk> out_buffer = it->get();
						out_buf->Append(*out_buffer, true);
						state.output_left = out_buf->size();
						state.base_offset = 0;
						res_collect.erase(it);
						context.client.num_of_async_tasks--;
						return ret;
					} catch (const std::exception &e) {
						throw InternalException(e.what());
					}
				}
			}
		}

		switch (controller->GetState()) {
		case BatchControllerState::SLICING: {
			batch_size = state.tuner.GetBatchSize();
			if (controller->HasNext(batch_size)) {
				if (is_async) {
					ret = AsyncNextEvalAdapt(context.client, state, batch_size, OperatorResultType::HAVE_MORE_OUTPUT,
					                         OperatorResultType::HAVE_MORE_OUTPUT);
				} else {
					ret = NextEvalAdapt(context.client, state, batch_size, chunk, OperatorResultType::HAVE_MORE_OUTPUT,
					                    OperatorResultType::HAVE_MORE_OUTPUT);
				}
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
				if (is_async) {
					ret = AsyncNextEvalAdapt(context.client, state, batch_size, OperatorResultType::HAVE_MORE_OUTPUT,
					                         OperatorResultType::HAVE_MORE_OUTPUT);
				} else {
					ret = NextEvalAdapt(context.client, state, batch_size, chunk, OperatorResultType::HAVE_MORE_OUTPUT,
					                    OperatorResultType::HAVE_MORE_OUTPUT);
				}
				controller->SetState(BatchControllerState::EMPTY);
			}
			break;
		}

		default:
			throw InternalException("BatchControlle State Unsupported");
		}

		return ret;
	} else if (kind == FunctionKind::ASYNC_PREDICTION) {
		return AsyncExecute(context.client, state_p, input, chunk, false, OperatorResultType::NEED_MORE_INPUT,
		                    OperatorResultType::HAVE_MORE_OUTPUT);
	} else if (kind == FunctionKind::SCHEDULE_PREDICTION) {
		state.executor.Execute(input, chunk);
		return OperatorResultType::NEED_MORE_INPUT;
	} else {
		throw InternalException("Unsupported Prediction Function Kind");
	}
}

template <typename RET_TYPE>
RET_TYPE PhysicalPredictionProjection::NextEvalAdapt(ClientContext &ctx, OperatorState &state, idx_t batch_size,
                                                     DataChunk &chunk, RET_TYPE ret_adapt, RET_TYPE no_adapt) const {
	auto &local = state.Cast<PredictionProjectionState>();
	auto &controller = local.controller;
	auto &out_buf = local.output_buffer;
	auto &tuner = local.tuner;
	RET_TYPE ret;
	auto &batch = controller->NextBatch(batch_size);
	controller->ExternalProjectionReset(ctx, *out_buf, local.executor);
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

template <typename RET_TYPE>
RET_TYPE PhysicalPredictionProjection::AsyncNextEvalAdapt(ClientContext &ctx, OperatorState &state, idx_t batch_size,
                                                          RET_TYPE need, RET_TYPE have) const {
	if (ctx.num_of_async_tasks.load() < ctx.upbound) {
		auto &local = state.Cast<PredictionProjectionState>();
		auto &controller = local.controller;
		auto &out_buf = local.output_buffer;
		auto &res_collect = local.res_collect;

		auto &batch = controller->NextBatch(batch_size);
		controller->ExternalProjectionReset(ctx, *out_buf, local.executor);

		unique_ptr<DataChunk> in_buffer = make_uniq<DataChunk>();
		in_buffer->Initialize(Allocator::Get(ctx), batch.GetTypes(), batch.GetCapacity());
		batch.Copy(*in_buffer);
		res_collect.push_back(
		    std::async(std::launch::async,
		               [&ctx, &controller, &local, in_buffer = std::move(in_buffer), &out_buf, &batch]() mutable {
			               return controller->AsyncExecuteExpression(ctx, local.executor, std::move(in_buffer),
			                                                         out_buf->GetTypes(), batch.GetCapacity());
		               }));
		ctx.num_of_async_tasks++;
		return need;
	}

	return have;
}

template <typename RET_TYPE>
RET_TYPE PhysicalPredictionProjection::AsyncExecute(ClientContext &context, OperatorState &state_p, DataChunk &input,
                                                    DataChunk &result, bool final_execute, RET_TYPE need_or_finish,
                                                    RET_TYPE have) const {
	auto &state = state_p.Cast<PredictionProjectionState>();
	auto &res_collect = state.res_collect;
	for (auto it = res_collect.begin(); it != res_collect.end(); ++it) {
		if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			try {
				unique_ptr<DataChunk> out_buffer = it->get();
				result.Append(*out_buffer);
				res_collect.erase(it);
				context.num_of_async_tasks--;
				return have;
			} catch (const std::exception &e) {
				throw InternalException(e.what());
			}
		}
	}

	if (final_execute) {
		if (res_collect.empty()) {
			return need_or_finish;
		} else {
			return have;
		}
	}

	if (context.num_of_async_tasks.load() < context.upbound) {
		unique_ptr<DataChunk> in_buffer = make_uniq<DataChunk>();
		in_buffer->Initialize(Allocator::Get(context), input.GetTypes(), STANDARD_VECTOR_SIZE);
		input.Copy(*in_buffer);
		res_collect.push_back(
		    std::async(std::launch::async, [&context, &state, in_buffer = std::move(in_buffer), &result]() mutable {
			    return state.controller->AsyncExecuteExpression(context, state.executor, std::move(in_buffer),
			                                                    result.GetTypes());
		    }));
		context.num_of_async_tasks++;
		return need_or_finish;
	}
	return have;
}

unique_ptr<OperatorState> PhysicalPredictionProjection::GetOperatorState(ExecutionContext &context) const {
	D_ASSERT(children.size() == 1);
	return make_uniq<PredictionProjectionState>(context, select_list, children[0]->GetTypes(), user_defined_size,
	                                            use_adaptive_size);
}

string PhysicalPredictionProjection::ParamsToString() const {
	string extra_info;
	for (auto &expr : select_list) {
		extra_info += expr->GetName() + "\n";
	}
	extra_info += use_adaptive_size ? "adaptive" : "prediction_size:" + std::to_string(user_defined_size) + "\n";
	extra_info += "UDF Kind : ";
	extra_info += (kind == FunctionKind::ASYNC_PREDICTION ? "ASYNC_PREDICTION" : "PREDICTION");
	extra_info += "\n";
	return extra_info;
}

OperatorFinalizeResultType PhysicalPredictionProjection::FinalExecute(ExecutionContext &context, DataChunk &chunk,
                                                                      GlobalOperatorState &gstate,
                                                                      OperatorState &state) const {
	auto &local = state.Cast<PredictionProjectionState>();
	if (kind == FunctionKind::BATCH_PREDICTION || kind == FunctionKind::ASYNC_BATCH_PREDICTION) {
		auto &controller = local.controller;

		auto &out_buf = local.output_buffer;

		auto &output_left = local.output_left;
		auto &base_offset = local.base_offset;

		idx_t batch_size = local.prediction_size;

		auto ret = OperatorFinalizeResultType::FINISHED;

		bool is_async = kind == FunctionKind::ASYNC_BATCH_PREDICTION;

		// batch adapting for the rest of output chunk
		if (output_left) {
			if (output_left <= STANDARD_VECTOR_SIZE) {
				controller->BatchAdapting(*out_buf, chunk, base_offset, output_left);
				output_left = 0;
				base_offset = 0;
				out_buf->Reset();
			} else {
				controller->BatchAdapting(*out_buf, chunk, base_offset);
				output_left -= STANDARD_VECTOR_SIZE;
				base_offset += STANDARD_VECTOR_SIZE;
			}
			ret = OperatorFinalizeResultType::HAVE_MORE_OUTPUT;

			return ret;
		}

		if (is_async) {
			auto &res_collect = local.res_collect;
			for (auto it = res_collect.begin(); it != res_collect.end(); ++it) {
				if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
					try {
						unique_ptr<DataChunk> out_buffer = it->get();
						out_buf->Append(*out_buffer, true);
						local.output_left = out_buf->size();
						local.base_offset = 0;
						res_collect.erase(it);
						context.client.num_of_async_tasks--;
						return OperatorFinalizeResultType::HAVE_MORE_OUTPUT;
					} catch (const std::exception &e) {
						throw InternalException(e.what());
					}
				}
			}
		}

		if (controller->HasNext(batch_size)) {
			if (is_async) {
				ret =
				    AsyncNextEvalAdapt(context.client, state, batch_size, OperatorFinalizeResultType::HAVE_MORE_OUTPUT,
				                       OperatorFinalizeResultType::HAVE_MORE_OUTPUT);
			} else {
				ret = NextEvalAdapt(context.client, local, batch_size, chunk,
				                    OperatorFinalizeResultType::HAVE_MORE_OUTPUT,
				                    OperatorFinalizeResultType::HAVE_MORE_OUTPUT);
			}
		} else {
			if (controller->GetSize() > 0) {
				if (is_async) {
					ret = AsyncNextEvalAdapt(context.client, state, controller->GetSize(),
					                         OperatorFinalizeResultType::HAVE_MORE_OUTPUT,
					                         OperatorFinalizeResultType::HAVE_MORE_OUTPUT);
				} else {
					ret = NextEvalAdapt(context.client, local, controller->GetSize(), chunk,
					                    OperatorFinalizeResultType::HAVE_MORE_OUTPUT,
					                    OperatorFinalizeResultType::FINISHED);
				}
			}
			if(is_async && local.res_collect.size()){
				ret = OperatorFinalizeResultType::HAVE_MORE_OUTPUT;
			}
		}

		return ret;
	} else if (kind == FunctionKind::ASYNC_PREDICTION) {
		return AsyncExecute(context.client, state, chunk, chunk, true, OperatorFinalizeResultType::FINISHED,
		                    OperatorFinalizeResultType::HAVE_MORE_OUTPUT);
	} else if (kind == FunctionKind::SCHEDULE_PREDICTION) {
		return OperatorFinalizeResultType::FINISHED;
	} else {
		throw InternalException("Unsupported Prediction Function Kind");
	}
}

} // namespace imbridge

} // namespace duckdb