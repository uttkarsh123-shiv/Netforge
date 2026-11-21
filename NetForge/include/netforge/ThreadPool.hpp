#pragma once
/**
 * NetForge — ThreadPool.hpp
 *
 * Fixed-size thread pool backed by TaskQueue.
 *
 * Design decisions:
 *  - Worker count is fixed at construction; resize() deliberately omitted
 *    to keep the invariant that every std::thread was created with the
 *    correct CPU affinity / NUMA domain (extendable later).
 *  - submit() returns a std::future<T> so callers can synchronise on results
 *    without coupling to std::thread directly.
 *  - RAII destructor: drains pending work then joins all threads.
 *  - Atomics track active task count for health metrics exposed via stats().
 */

#include "TaskQueue.hpp"
#include "Logger.hpp"

#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace netforge {

class ThreadPool {
public:
    struct Stats {
        std::size_t workerCount;
        std::size_t queuedTasks;
        std::size_t activeTasks;
        std::size_t completedTasks;
    };

    explicit ThreadPool(std::size_t workerCount) {
        if (workerCount == 0)
            throw std::invalid_argument("ThreadPool: workerCount must be > 0");

        workers_.reserve(workerCount);
        for (std::size_t i = 0; i < workerCount; ++i)
            workers_.emplace_back([this, i] { workerLoop(i); });

        NF_INFO("ThreadPool started with ", workerCount, " workers");
    }

    ~ThreadPool() {
        shutdown();
        for (auto& t : workers_)
            if (t.joinable()) t.join();
        NF_INFO("ThreadPool stopped. Completed tasks: ", completedTasks_.load());
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Submit any callable with arguments; returns std::future<ReturnType>.
     *
     * Example:
     *   auto fut = pool.submit([]{ return 42; });
     *   int result = fut.get();
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        // Wrap in a packaged_task for future support, then type-erase.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f),
             targs = std::make_tuple(std::forward<Args>(args)...)]() mutable
            { return std::apply(std::move(func), std::move(targs)); }
        );

        std::future<ReturnType> fut = task->get_future();

        bool enqueued = queue_.push([task, this] {
            activeTasks_.fetch_add(1, std::memory_order_relaxed);
            (*task)();
            activeTasks_.fetch_sub(1, std::memory_order_relaxed);
            completedTasks_.fetch_add(1, std::memory_order_relaxed);
        });

        if (!enqueued)
            throw std::runtime_error("ThreadPool: submit after shutdown");

        return fut;
    }

    // Graceful shutdown: drains queue then signals workers.
    void shutdown() {
        queue_.shutdown();
    }

    [[nodiscard]] Stats stats() const {
        return {
            workers_.size(),
            queue_.size(),
            activeTasks_.load(std::memory_order_relaxed),
            completedTasks_.load(std::memory_order_relaxed)
        };
    }

private:
    void workerLoop(std::size_t id) {
        NF_DEBUG("Worker[", id, "] started");
        while (auto task = queue_.pop()) {
            try {
                (*task)();
            } catch (const std::exception& ex) {
                NF_ERROR("Worker[", id, "] uncaught exception: ", ex.what());
            } catch (...) {
                NF_ERROR("Worker[", id, "] unknown exception");
            }
        }
        NF_DEBUG("Worker[", id, "] stopped");
    }

    TaskQueue                queue_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> activeTasks_{0};
    std::atomic<std::size_t> completedTasks_{0};
};

} // namespace netforge
