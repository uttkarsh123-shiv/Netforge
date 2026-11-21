/**
 * Tests for ConnectionManager.
 */

#include "netforge/ConnectionManager.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using netforge::ConnectionManager;
using netforge::ConnectionInfo;

static auto makeInfo(int fd) {
    return std::make_shared<ConnectionInfo>(
        fd,
        "127.0.0.1",
        static_cast<uint16_t>(10000 + fd)
    );
}

TEST(ConnectionManager, AddAndCount) {
    ConnectionManager m;
    m.add(makeInfo(1));
    m.add(makeInfo(2));
    EXPECT_EQ(m.count(), 2u);
}

TEST(ConnectionManager, RemoveDecrementsCount) {
    ConnectionManager m;
    m.add(makeInfo(1));
    m.remove(1);
    EXPECT_EQ(m.count(), 0u);
}

TEST(ConnectionManager, GetReturnsCorrectInfo) {
    ConnectionManager m;
    auto info = makeInfo(5);
    m.add(info);
    auto got = m.get(5);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->fd, 5);
}

TEST(ConnectionManager, GetMissingReturnsNull) {
    ConnectionManager m;
    EXPECT_EQ(m.get(99), nullptr);
}

TEST(ConnectionManager, SnapshotSize) {
    ConnectionManager m;
    for (int i = 0; i < 10; ++i) m.add(makeInfo(i));
    EXPECT_EQ(m.snapshot().size(), 10u);
}

TEST(ConnectionManager, ConcurrentAddRemove) {
    ConnectionManager m;
    constexpr int kPerThread = 500;
    constexpr int kThreads   = 4;

    std::vector<std::thread> adders, removers;
    std::atomic<int> base{0};

    for (int t = 0; t < kThreads; ++t) {
        adders.emplace_back([&, t] {
            for (int i = 0; i < kPerThread; ++i) {
                int fd = t * kPerThread + i;
                m.add(makeInfo(fd));
            }
        });
    }
    for (auto& th : adders) th.join();

    EXPECT_EQ(m.count(), static_cast<std::size_t>(kThreads * kPerThread));

    for (int t = 0; t < kThreads; ++t) {
        removers.emplace_back([&, t] {
            for (int i = 0; i < kPerThread; ++i) {
                int fd = t * kPerThread + i;
                m.remove(fd);
            }
        });
    }
    for (auto& th : removers) th.join();

    EXPECT_EQ(m.count(), 0u);
}
