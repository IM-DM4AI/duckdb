#pragma once

#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/planner/expression.hpp"

namespace duckdb {

namespace prediction {

class PredictionProjectionGlobalState;

class PhysicalPredictionProjection : public PhysicalOperator {
public:
	static constexpr const PhysicalOperatorType TYPE = PhysicalOperatorType::PREDICTION_PROJECTION;

public:
	PhysicalPredictionProjection(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
	                   idx_t estimated_cardinality, idx_t user_defined_size = INITIAL_PREDICTION_SIZE, FunctionKind kind = FunctionKind::PREDICTION);

	vector<unique_ptr<Expression>> select_list;
	idx_t user_defined_size;
	bool use_adaptive_size;
	bool caching_supported;
	FunctionKind kind;

	PredictionGlobalState* pgstate;

	std::function<OperatorResultType(ExecutionContext&, DataChunk&,
		 DataChunk&, GlobalOperatorState&, OperatorState&)> exec_func;

	std::function<OperatorFinalizeResultType(ExecutionContext&,
		DataChunk&, GlobalOperatorState&, OperatorState&)> final_exec_func;
		
	OperatorResultType BatchingExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
			GlobalOperatorState &gstate, OperatorState &state_p) const;
	OperatorFinalizeResultType BatchingFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
				OperatorState &state) const;
	
	OperatorResultType ProcessExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
			GlobalOperatorState &gstate, OperatorState &state_p) const;
	OperatorFinalizeResultType ProcessFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
			OperatorState &state) const;

	OperatorResultType ProcessSchedExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
			GlobalOperatorState &gstate, OperatorState &state_p) const;
	OperatorFinalizeResultType ProcessSchedFinalExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
			OperatorState &state) const;
	
	OperatorResultType ProcessSchedPoolExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
			GlobalOperatorState &gstate, OperatorState &state_p) const;
	OperatorFinalizeResultType ProcessSchedFinalPoolExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
			OperatorState &state) const;

	OperatorResultType ProcessSchedPoolWithBatchingExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const;

	OperatorFinalizeResultType ProcessSchedFinalPoolWithBatchingExec(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
    OperatorState &state) const;

public:
	unique_ptr<OperatorState> GetOperatorState(ExecutionContext &context) const override;
	unique_ptr<GlobalOperatorState> GetGlobalOperatorState(ClientContext &context) const override;
	OperatorResultType Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
	                           GlobalOperatorState &gstate, OperatorState &state) const override;
	bool CanCacheType(const LogicalType &type);

	template<typename RET_TYPE>
	RET_TYPE NextEvalAdapt(OperatorState &state, idx_t batch_size, DataChunk &chunk, RET_TYPE ret_adapt, RET_TYPE no_adapt) const;

	template<typename RET_TYPE>
	RET_TYPE NextEvalAdaptWithSchedule(OperatorState &state, GlobalOperatorState &gstate, idx_t batch_size, DataChunk &chunk, RET_TYPE ret_adapt, RET_TYPE no_adapt) const;

	bool ParallelOperator() const override {
		return true;
	}

	bool RequiresFinalExecute() const {
		return true;
	}

	~PhysicalPredictionProjection();

	OperatorFinalizeResultType FinalExecute(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
	                                        OperatorState &state) const final;

	string ParamsToString() const override;
    
};

} // namespace prediction

}// namespace duckdb