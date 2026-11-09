#ifndef LOCKSTEP_ASSETS_ORDERED_WORK_HPP
#define LOCKSTEP_ASSETS_ORDERED_WORK_HPP

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace lockstep::assets
{
    // requested: a worker count, 0 for the hardware thread count.
    // jobs:      how many jobs there are.
    // Returns:   the workers to start: at least 1, at most one per job.
    [[nodiscard]] inline uint32_t workerCountFor(uint32_t requested, std::size_t jobs)
    {
        const uint32_t wanted = requested != 0 ? requested : std::max(1u, std::thread::hardware_concurrency());
        return static_cast<uint32_t>(std::min<std::size_t>(wanted, std::max<std::size_t>(jobs, 1)));
    }

    // Runs `produce(index)` for every index in [0, count) on worker threads and hands each result
    // to `consume(index, result)` on the calling thread, in index order. Workers run at most
    // `ahead` jobs past the last consumed one, which bounds how many results wait in memory.
    // Used where loading is parallel but GPU uploads must stay on one thread.
    // count:   number of jobs.
    // workers: worker threads (see workerCountFor()).
    // ahead:   how far workers may run ahead of the consumer; at least 1.
    // produce: T(std::size_t index); called concurrently, must be thread-safe.
    // consume: void(std::size_t index, T&& result); called on this thread.
    template<typename T, typename Produce, typename Consume>
    void runOrdered(std::size_t count, uint32_t workers, std::size_t ahead, Produce&& produce, Consume&& consume)
    {
        std::mutex mutex;
        std::condition_variable changed;
        std::size_t nextJob = 0;
        std::size_t consumed = 0;
        std::vector<std::optional<T>> results(count);
        ahead = std::max<std::size_t>(ahead, 1);

        const auto work = [&]
        {
            while (true)
            {
                std::size_t job = 0;

                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [&] { return nextJob >= count || nextJob < consumed + ahead; });

                    if (nextJob >= count)
                    {
                        return;
                    }

                    job = nextJob++;
                }

                T result = produce(job);

                {
                    std::lock_guard lock(mutex);
                    results[job].emplace(std::move(result));
                }

                changed.notify_all();
            }
        };

        std::vector<std::jthread> threads;
        threads.reserve(workers);

        for (uint32_t worker = 0; worker < workers; ++worker)
        {
            threads.emplace_back(work);
        }

        for (std::size_t job = 0; job < count; ++job)
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return results[job].has_value(); });
            T result = std::move(*results[job]);
            results[job].reset();
            consumed = job + 1;
            lock.unlock();
            changed.notify_all();
            consume(job, std::move(result));
        }
    }
} //namespace lockstep::assets

#endif //LOCKSTEP_ASSETS_ORDERED_WORK_HPP
