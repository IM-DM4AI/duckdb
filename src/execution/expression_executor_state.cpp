#include "duckdb/execution/expression_executor_state.hpp"

#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

void ExpressionState::AddChild(Expression *expr, idx_t capacity) {
	types.push_back(expr->return_type);
	child_states.push_back(ExpressionExecutor::InitializeState(*expr, root, capacity));
}

void ExpressionState::Finalize(bool empty, idx_t capacity) {
	if (types.empty()) {
		return;
	}
	if (empty) {
		intermediate_chunk.InitializeEmpty(types);
		intermediate_chunk.SetCapacity(capacity);
	} else {
		intermediate_chunk.Initialize(GetAllocator(), types, capacity);
	}
}

unique_ptr<ExpressionState> ExpressionState::CopyState() {
	unique_ptr<ExpressionState> new_state =
	    ExpressionExecutor::InitializeState(expr, root, intermediate_chunk.GetCapacity());
	new_state->types = types;
	for (auto &child : child_states) {
		new_state->child_states.push_back(child->CopyState());
	}
	return new_state;
}

void ExpressionState::UpdateCapacity(ClientContext &context, idx_t capacity) {
	if (capacity > intermediate_chunk.GetCapacity()) {

		for (idx_t i = 0; i < intermediate_chunk.ColumnCount(); i++) {
			intermediate_chunk.data[i].Resize(0, capacity);
			intermediate_chunk.ResizeCache(context, i, capacity);
		}
		intermediate_chunk.SetCapacity(capacity);
	}
	for (auto &state : child_states) {
		state->UpdateCapacity(context, capacity);
	}
}

Allocator &ExpressionState::GetAllocator() {
	return root.executor->GetAllocator();
}

bool ExpressionState::HasContext() {
	return root.executor->HasContext();
}

ClientContext &ExpressionState::GetContext() {
	if (!HasContext()) {
		throw BinderException("Cannot use %s in this context", (expr.Cast<BoundFunctionExpression>()).function.name);
	}
	return root.executor->GetContext();
}

ExpressionState::ExpressionState(const Expression &expr, ExpressionExecutorState &root) : expr(expr), root(root) {
}

ExpressionExecutorState::ExpressionExecutorState() {
}

void ExpressionState::Verify(ExpressionExecutorState &root_executor) {
	D_ASSERT(&root_executor == &root);
	for (auto &entry : child_states) {
		entry->Verify(root_executor);
	}
}

void ExpressionExecutorState::Verify() {
	D_ASSERT(executor);
	root_state->Verify(*this);
}

} // namespace duckdb
