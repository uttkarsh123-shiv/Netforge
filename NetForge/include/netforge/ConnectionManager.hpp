#pragma once
/**
 * NetForge — ConnectionManager.hpp
 *
 * Tracks open client connections with thread-safe access.
 *
 * Design decisions:
 *  - Keyed by file descriptor (int) — unique per process lifetime.
 *  - shared_ptr<ConnectionInfo> lets the server and stats reader share
 *    ownership without copying the struct.
 *  - Intentionally lightweight: the actual I/O lives in ConnectionHandler.
 */

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace netforge {

struct ConnectionInfo {
    int         fd;
    std::string remoteAddr;
    uint16_t    remotePort;
    std::atomic<uint64_t> requestsHandled{0};

    ConnectionInfo() = default;
    ConnectionInfo(int fd_, std::string addr, uint16_t port)
        : fd(fd_), remoteAddr(std::move(addr)), remotePort(port) {}
};

class ConnectionManager {
public:
    using InfoPtr = std::shared_ptr<ConnectionInfo>;

    void add(InfoPtr info) {
        std::lock_guard lock(mutex_);
        connections_.emplace(info->fd, std::move(info));
    }

    void remove(int fd) {
        std::lock_guard lock(mutex_);
        connections_.erase(fd);
    }

    [[nodiscard]] std::size_t count() const {
        std::lock_guard lock(mutex_);
        return connections_.size();
    }

    [[nodiscard]] InfoPtr get(int fd) const {
        std::lock_guard lock(mutex_);
        auto it = connections_.find(fd);
        return (it != connections_.end()) ? it->second : nullptr;
    }

    // Snapshot of all current connections (safe to iterate after returning)
    [[nodiscard]] std::vector<InfoPtr> snapshot() const {
        std::lock_guard lock(mutex_);
        std::vector<InfoPtr> result;
        result.reserve(connections_.size());
        for (auto& [fd, info] : connections_)
            result.push_back(info);
        return result;
    }

private:
    mutable std::mutex                      mutex_;
    std::unordered_map<int, InfoPtr>        connections_;
};

} // namespace netforge
