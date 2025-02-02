#pragma once

#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/planner/expression.hpp"

namespace duckdb {

namespace imbridge {

class PhysicalPredictionProjection : public PhysicalOperator {
public:
	static constexpr const PhysicalOperatorType TYPE = PhysicalOperatorType::PREDICTION_PROJECTION;

public:
	PhysicalPredictionProjection(vector<LogicalType> types, vector<unique_ptr<Expression>> select_list,
	                             idx_t estimated_cardinality, idx_t user_defined_size = INITIAL_PREDICTION_SIZE,
	                             FunctionKind kind = FunctionKind::BATCH_PREDICTION);

	vector<unique_ptr<Expression>> select_list;
	idx_t user_defined_size;
	bool use_adaptive_size;
	FunctionKind kind;

public:
	unique_ptr<OperatorState> GetOperatorState(ExecutionContext &context) const override;
	OperatorResultType Execute(ExecutionContext &context, DataChunk &input, DataChunk &chunk,
	                           GlobalOperatorState &gstate, OperatorState &state) const override;

	template <typename RET_TYPE>
	RET_TYPE AsyncExecute(ClientContext &context, OperatorState &state, DataChunk &input, DataChunk &result,
	                      bool final_execute, RET_TYPE need_or_finish, RET_TYPE have) const;

	template <typename RET_TYPE>
	RET_TYPE AsyncNextEvalAdapt(ClientContext &ctx, OperatorState &state, idx_t batch_size, RET_TYPE need,
	                            RET_TYPE have) const;

	template <typename RET_TYPE>
	RET_TYPE NextEvalAdapt(ClientContext &ctx, OperatorState &state, idx_t batch_size, DataChunk &chunk,
	                       RET_TYPE ret_adapt, RET_TYPE no_adapt) const;

	bool ParallelOperator() const override {
		return true;
	}

	bool RequiresFinalExecute() const {
		return true;
	}

	OperatorFinalizeResultType FinalExecute(ExecutionContext &context, DataChunk &chunk, GlobalOperatorState &gstate,
	                                        OperatorState &state) const final;

	string ParamsToString() const override;
};

} // namespace imbridge

} // namespace duckdb