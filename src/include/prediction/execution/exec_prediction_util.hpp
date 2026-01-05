#pragma once

#include <list>

#include <unistd.h>
#include "duckdb/execution/physical_operator_states.hpp"

#include "prediction/params.hpp"
#include "prediction/execution/batch_controller.hpp"
#include "prediction/execution/adaptive_batch_tuner.hpp"

#include <cstdio>

namespace IMLane {
    namespace DBEnd {
        class DBEndContext;
        struct IMSettings;
    }
}

namespace duckdb {

    namespace prediction {

        struct LaneSchedQueue;

        static bool core_aware = true; 

        static int GetCurrentCpu() {
            return sched_getcpu();
        }

        enum class SchedState: uint8_t {SCHEDULE,  OUTPUT};

        class LaneScheduler {
        public:
            LaneScheduler();
            void Enqueue(int id);
            bool TryDequeue(int &id);
            ~LaneScheduler();
        private:
            LaneSchedQueue* queue;
        };

        class PredictionGlobalState {
            public:
                PredictionGlobalState(FunctionKind kind);
                ~PredictionGlobalState();
    
                bool IMLaneOptimize() {
                    return kind == FunctionKind::PROCESS_PREDICTION || 
                    kind == FunctionKind::SCHEDULE_PREDICTION ||
                    kind == FunctionKind::THREAD_SCHEDULE_PREDICTION ||
                    kind == FunctionKind::THREAD_SCHEDULE_PREDICTION_WITH_BATCHING;
                }
    
                IMLane::DBEnd::DBEndContext* lane_context;
                FunctionKind kind;

        };
        
        class PredictionState : public OperatorState {
        public:
        
            // for multiple expressions
            explicit PredictionState(ExecutionContext &context,const vector<LogicalType> &input_types, 
                PredictionGlobalState* pgstate,
            const vector<unique_ptr<Expression>> &expressions, bool adaptive,
            idx_t prediction_size = INITIAL_PREDICTION_SIZE, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
                : tuner(prediction_size, adaptive), prediction_size(prediction_size),
                 padded(0), output_left(0), base_offset(0), pgstate(pgstate) {
                    controller = make_uniq<BatchController>();
                    controller->Initialize(Allocator::Get(context.client), input_types,  buffer_capacity);

                    sched_state = SchedState::SCHEDULE;
            }
        
            // for single expression
            explicit PredictionState(ExecutionContext &context,const vector<LogicalType> &input_types,
                PredictionGlobalState* pgstate, 
            Expression &expr, bool adaptive,
            idx_t prediction_size = INITIAL_PREDICTION_SIZE, idx_t buffer_capacity = DEFAULT_RESERVED_CAPACITY)
                : tuner(prediction_size, adaptive), prediction_size(prediction_size),
                 padded(0), output_left(0), base_offset(0), pgstate(pgstate) {
                    controller = make_uniq<BatchController>();
                    controller->Initialize(Allocator::Get(context.client), input_types,  buffer_capacity);

                    sched_state = SchedState::SCHEDULE;
            }
        
            // IMBridge Optimization
            AdaptiveBatchTuner tuner;
            unique_ptr<BatchController> controller;
            idx_t prediction_size;
        
            idx_t padded;
        
            // slicing range for batch adapting
            idx_t output_left;
            idx_t base_offset;

            // IMLane Optimization
            PredictionGlobalState* pgstate;
            std::list<int> sched_slot_ids;
            SchedState sched_state;
        
        };

        class PredictionOpGlobalState: public GlobalOperatorState {
        public:
            unique_ptr<LaneScheduler> sched;
            IMLane::DBEnd::IMSettings* settings;

            PredictionOpGlobalState(IMLane::DBEnd::IMSettings settings);

            ~PredictionOpGlobalState();
        };

    } // namespace prediction

} // namespace duckdb