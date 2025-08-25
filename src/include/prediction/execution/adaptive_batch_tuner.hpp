#pragma once
#include "prediction/plan_prediction_util.hpp"
#include <chrono>

namespace duckdb {
namespace prediction {

class AdaptiveBatchTuner {

public:
    explicit AdaptiveBatchTuner(idx_t init_batch_size, bool adaptive = false);

    idx_t GetBatchSize();

    void StartProfile();
    void EndProfile();

private:
    //! used for adaptive prediction batch size acquisition
    idx_t batch_size;
    idx_t prev_batch_size;
    std::chrono::time_point<std::chrono::steady_clock>  start_time;
    double prediction_speed;

    //! parameters of the tuning algorithm
    idx_t stride;
    double smooth_factor;
    double stand_factor;
    idx_t stand_rounds;
    bool stop;
};

} // namespace prediction
    
} // namespace duckdb
