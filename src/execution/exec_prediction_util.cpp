#include "prediction/execution/exec_prediction_util.hpp"

#include "dbend/c/imlane_dbend.hpp"

namespace duckdb{

    namespace prediction {

        PredictionGlobalState::PredictionGlobalState(FunctionKind kind) {
            this->kind = kind;
            lane_context = nullptr;
            if(kind == FunctionKind::PROCESS_PREDICTION || 
                kind == FunctionKind::SCHEDULE_PREDICTION) {
                    lane_context = new IMLane::DBEnd::DBEndContext();
                    auto setup_status = lane_context->Setup();
                    if(!(setup_status.kind == IMLane::DBEnd::StatusKind::OK)) {
                        InternalException("IMLane: Fail to setup dbend context.");
                    }
            }
        }

        PredictionGlobalState::~PredictionGlobalState() {
            delete lane_context;
        }
    }
}
