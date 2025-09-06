#include "prediction/execution/exec_prediction_util.hpp"

#include "dbend/c/imlane_dbend.hpp"

#include "concurrentqueue.h"
#include "lightweightsemaphore.h"
#include <queue>
#include <mutex>

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

        typedef duckdb_moodycamel::LightweightSemaphore lightweight_semaphore_t;
        typedef duckdb_moodycamel::ConcurrentQueue<int> sched_id_queue_t;

        struct LaneSchedQueue {
            std::queue<int> q;
            std::mutex mtx;
            lightweight_semaphore_t semaphore;
        };

        void LaneScheduler::Enqueue(int id) {
            std::lock_guard<std::mutex> guard(queue->mtx);
            queue->q.push(id);
        }
        bool LaneScheduler::TryDequeue(int &id) {
            std::lock_guard<std::mutex> guard(queue->mtx);
            if(queue->q.empty()) {
                return false;
            } else {
                id = queue->q.front();
                queue->q.pop();
                return true;
            }
        }

        LaneScheduler::LaneScheduler(){
            queue = new LaneSchedQueue();
        }

        LaneScheduler::~LaneScheduler() {
            delete queue;
        }

        PredictionOpGlobalState::PredictionOpGlobalState(IMLane::DBEnd::IMSettings settings){
            sched = make_uniq<LaneScheduler>();
            this->settings = new IMLane::DBEnd::IMSettings(settings);
        }

        
        PredictionOpGlobalState::~PredictionOpGlobalState() {
            delete settings;
        }
    }
}
