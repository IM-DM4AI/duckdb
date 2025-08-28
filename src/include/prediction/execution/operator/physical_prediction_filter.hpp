#pragma once

#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/planner/expression.hpp"

namespace duckdb {

namespace prediction {

class PhysicalPredictionFilter : public PhysicalOperator {
public:
	static constexpr const PhysicalOperatorType TYPE = PhysicalOperatorType::PREDICTION_FILTER;

public:
	PhysicalPredictionFilter(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
     idx_t estimated_cardinality, idx_t prediction_size, FunctionKind kind = FunctionKind::PREDICTION);

	//! The filter expression
	unique_ptr<Expression> expression;
    idx_t user_defined_size;
	bool use_adaptive_size;
	bool caching_supported;
	FunctionKind kind;

	PredictionGlobalState* pgstate;

	std::function<OperatorResultType(ExecutionContext&, DataChunk&,
		DataChunk&, GlobalOperatorState&, OperatorState&)> exec_func;

   OperatorResultType BatchingExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
		   GlobalOperatorState &gstate, OperatorState &state) const;
   
   OperatorResultType ProcessExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
		   GlobalOperatorState &gstate, OperatorState &state) const;

   OperatorResultType ProcessSchedExec(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
		   GlobalOperatorState &gstate, OperatorState &state) const;

public:
	unique_ptr<OperatorState> GetOperatorState(ExecutionContext &context) const override;
	unique_ptr<GlobalOperatorState> GetGlobalOperatorState(ClientContext &context) const override;
	
	bool CanCacheType(const LogicalType &type);

	template<typename RET_TYPE>
	RET_TYPE NextEvalAdapt(OperatorState &state, idx_t batch_size, DataChunk &chunk, RET_TYPE ret_adapt, RET_TYPE no_adapt) const;

	bool ParallelOperator() const override {
		return true;
	}

	bool RequiresFinalExecute() const {
		return true;
	}

	~PhysicalPredictionFilter() {
		delete pgstate;
	}

	OperatorFinalizeResultType FinalExecute(ExecutionContext &context, 
	DataChunk &chunk, GlobalOperatorState &gstate, OperatorState &state) const final;

	string ParamsToString() const override;

protected:
	OperatorResultType Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
	                                   GlobalOperatorState &gstate, OperatorState &state) const override;
};

} // namespace prediction

} // namespace duckdb