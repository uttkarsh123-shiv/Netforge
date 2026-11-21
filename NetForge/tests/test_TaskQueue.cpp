/**
 * Tests for TaskQueue.
 *
 * Covers:
 *  - Single push/pop roundtrip
 *  - Multi-threaded concurrent producers and consumers
 *  - Shutdown drains remaining items then returns nullopt
 *  - Push after shutdown returns false
 */

#include "netforge/TaskQueue.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

using netforge::TaskQueue;

TEST(TaskQueue, PushPopSingleThread) {
    TaskQueue q;
    bool ran = false;
    q.push([&ran]{ ran = true; });

    auto task = q.pop();
    ASSERT_TRUE(task.has_value());
    (*task)();
    EXPECT_TRUE(ran);
}

TEST(TaskQueue, ShutdownMakesPopReturnNullopt) {
    TaskQueue q;
    q.shutdown();
    auto task = q.pop();
    EXPECT_FALSE(task.has_value());
}

TEST(TaskQueue, PushAfterShutdownReturnsFalse) {
    TaskQueue q;
    q.shutdown();
    bool ok = q.push([]{ });
    EXPECT_FALSE(ok);
}

TEST(TaskQueue, DrainBeforeNullopt) {
    // Items already in the queue should be delivered before the sentinel.
    TaskQueue q;
    int counter = 0;
    for (int i = 0; i < 5; ++i)
        q.push([&counter]{ ++counter; });

    q.shutdown();

    int popped = 0;
    while (auto t = q.pop()) {
        (*t)();
        ++popped;
    }
    EXPECT_EQ(popped, 5);
    EXPECT_EQ(counter, 5);
}

TEST(TaskQueue, ConcurrentProducersAndConsumers) {
    TaskQueue q;
    std::atomic<int> sum{0};

    constexpr int kProducers = 4;
    constexpr int kPerProd   = 1000;
    constexpr int kConsumers = 4;

    std::vector<std::thread> producers, consumers;

    // Consumers: pop until they receive kProducers*kPerProd total tasks.
    std::atomic<int> consumed{0};
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (consumed.load() < kProducers * kPerProd) {
                auto t = q.pop();
                if (!t) break;   // shutdown
                (*t)();
                consumed.fetch_add(1);
            }
        });
    }

    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&] {
            for (int i = 0; i < kPerProd; ++i)
                q.push([&sum]{ sum.fetch_add(1); });
        });
    }

    for (auto& t : producers) t.join();

    // Let consumers drain then shutdown.
    // Busy-wait with a sleep is fine in a test.
    while (q.size() > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    q.shutdown();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(sum.load(), kProducers * kPerProd);
}
