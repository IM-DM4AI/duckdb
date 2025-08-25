#include "prediction/params.hpp"
#include "prediction/execution/adaptive_batch_tuner.hpp"
#include <chrono>

namespace duckdb
{
    namespace prediction
    {
        static const idx_t default_stride = 256;
        static const double default_smooth_factor = 0.25;
        static const double default_stand_factor = 0.1;
        static const idx_t default_stand_rounds = 10;

        idx_t AdaptiveBatchTuner::GetBatchSize() {
            return batch_size;
        }

        AdaptiveBatchTuner::AdaptiveBatchTuner(idx_t init_batch_size, bool adaptive): batch_size(init_batch_size) {
            if (adaptive) {
                prev_batch_size = batch_size;
                stride = default_stride;
                smooth_factor = default_smooth_factor;
                stand_factor = default_stand_factor;
                stand_rounds = default_stand_rounds;

                prediction_speed = 0;

                stop = false;
            } else {
                stop = true;
            }
        }

        void AdaptiveBatchTuner::StartProfile() {
            start_time = std::chrono::steady_clock::now();
        }

        void AdaptiveBatchTuner::EndProfile() {
            auto stop_time = std::chrono::steady_clock::now();
            double duration_ms = std::chrono::duration<double, std::milli>(stop_time - start_time).count();

            double current_speed = batch_size/duration_ms;
            if (!stop) {
                if (current_speed > (1 + stand_factor) * prediction_speed) {
                    prev_batch_size = batch_size;
                    batch_size += stride;
                    if (batch_size > DEFAULT_RESERVED_CAPACITY) {
                        batch_size = DEFAULT_RESERVED_CAPACITY;
                        stop = true;
                    }
                } else {
                    prev_batch_size = batch_size;
                    batch_size = (idx_t) (batch_size*(1-smooth_factor) + prev_batch_size*smooth_factor);

                    stand_rounds -= 1;
                    if (stand_rounds <= 0){
                        stop = true;
                    }
                }
            }
            prediction_speed = current_speed;
        }

    } // namespace prediction
    
} // namespace duckdb
