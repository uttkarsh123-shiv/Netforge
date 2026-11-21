/**
 * Tests for ThreadPool.
 *
 * Covers:
 *  - submit() returns correct future value
 *  - Exceptions in tasks don't crash worker thread
 *  - Many tasks complete within timeout
 *  - stats() reflects activity
 *  - Zero workers throws at construction
 */

#include "netforge/ThreadPool.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <vector>

using netforge::ThreadPool;

TEST(ThreadPool, FutureValue) {
    ThreadPool pool(2);
    auto fut = pool.submit([]{ return 42; });
    EXPECT_EQ(fut.get(), 42);
}

TEST(ThreadPool, ManyTasksComplete) {
    ThreadPool pool(4);
    constexpr int kTasks = 10'000;
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futs;
    futs.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i)
        futs.push_back(pool.submit([&counter]{ counter.fetch_add(1); }));

    for (auto& f : futs) f.get();
    EXPECT_EQ(counter.load(), kTasks);
}

TEST(ThreadPool, ExceptionInTaskDoesNotCrash) {
    ThreadPool pool(2);
    // A task that throws should log the error; the future carries the exception.
    auto fut = pool.submit([]() -> int {
        throw std::runtime_error("test exception");
        return 0;
    });

    EXPECT_THROW(fut.get(), std::runtime_error);
    // Pool is still alive — submit another task.
    auto fut2 = pool.submit([]{ return 7; });
    EXPECT_EQ(fut2.get(), 7);
}

TEST(ThreadPool, ZeroWorkersThrows) {
    EXPECT_THROW(ThreadPool pool(0), std::invalid_argument);
}

TEST(ThreadPool, StatsReflectCompletedTasks) {
    ThreadPool pool(2);
    constexpr int kTasks = 100;
    std::vector<std::future<void>> futs;
    for (int i = 0; i < kTasks; ++i)
        futs.push_back(pool.submit([]{ }));

    for (auto& f : futs) f.get();

    auto s = pool.stats();
    EXPECT_EQ(s.completedTasks, static_cast<std::size_t>(kTasks));
    EXPECT_EQ(s.workerCount,    2u);
}

TEST(ThreadPool, SubmitWithArgs) {
    ThreadPool pool(1);
    auto fut = pool.submit([](int a, int b){ return a + b; }, 10, 32);
    EXPECT_EQ(fut.get(), 42);
}
