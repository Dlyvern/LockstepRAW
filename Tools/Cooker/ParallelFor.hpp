#ifndef LOCKSTEP_COOKER_PARALLEL_FOR_HPP
#define LOCKSTEP_COOKER_PARALLEL_FOR_HPP

#include "Assets/OrderedWork.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace lockstep::cooker
{
    // Calls `job(index)` for every index in [0, count) on worker threads and waits for all.
    // Jobs write their results to per-index slots, which keeps the output independent of
    // scheduling.
    // count:   number of jobs.
    // threads: worker threads; 0 picks the hardware thread count.
    // job:     void(std::size_t index); called concurrently.
    template<typename Job>
    void parallelFor(std::size_t count, uint32_t threads, Job&& job)
    {
        std::atomic<std::size_t> next{0};
        const uint32_t workers = assets::workerCountFor(threads, count);
        std::vector<std::jthread> pool;
        pool.reserve(workers);

        for (uint32_t worker = 0; worker < workers; ++worker)
        {
            pool.emplace_back([&next, count, &job]
            {
                for (std::size_t index = next++; index < count; index = next++)
                {
                    job(index);
                }
            });
        }
    }
} //namespace lockstep::cooker

#endif //LOCKSTEP_COOKER_PARALLEL_FOR_HPP
