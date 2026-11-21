#pragma once
/**
 * NetForge — TaskQueue.hpp
 *
 * A thread-safe, unbounded MPMC task queue backed by std::queue.
 *
 * Design decisions:
 *  - std::function<void()> as the task type — erases the callable so the
 *    thread pool doesn't need to know about the work's signature.
 *  - condition_variable::wait with a predicate guards against spurious wakeups.
 *  - shutdown() unblocks all waiting threads; subsequent push() calls are
 *    silently dropped so producers can drain gracefully.
 */

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>

namespace netforge {

class TaskQueue {
public:
    using Task = std::function<void()>;

    TaskQueue() = default;

    // Non-copyable, non-movable (owns synchronisation primitives)
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    // Push a task; returns false if the queue has been shut down.
    bool push(Task task) {
        {
            std::unique_lock lock(mutex_);
            if (shutdown_) return false;
            queue_.push(std::move(task));
        }
        cv_.notify_one();
        return true;
    }

    // Block until a task is available or the queue shuts down.
    // Returns std::nullopt when shut down AND the queue is drained.
    std::optional<Task> pop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });

        if (queue_.empty()) return std::nullopt;   // shut down & drained

        Task t = std::move(queue_.front());
        queue_.pop();
        return t;
    }

    // Signal all waiting threads to wake and return nullopt.
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::queue<Task>        queue_;
    bool                    shutdown_{false};
};

} // namespace netforge
