#pragma once
/**
 * NetForge — EpollPoller.hpp
 *
 * Cross-platform I/O readiness notification.
 *
 *   Linux   → epoll (edge-triggered)
 *   Windows → select() with a timeout
 *
 * The public API is identical on both platforms so TcpServer.hpp needs no
 * platform #ifdefs.
 *
 * ReadyEvent mirrors enough of epoll_event that callers work unchanged.
 */

#include "Platform.hpp"

#include <stdexcept>
#include <system_error>
#include <vector>
#include <cstring>

#ifndef _WIN32
  #include <sys/epoll.h>
  #include <unistd.h>
#endif

namespace netforge {

struct ReadyEvent {
    int      fd{-1};
    uint32_t events{0};   // READY_READ | READY_WRITE

    static constexpr uint32_t READY_READ  = 1u;
    static constexpr uint32_t READY_WRITE = 2u;
    static constexpr uint32_t READY_ERR   = 4u;
};

// ═══════════════════════════════════════════════════════════════════════════
// Linux path — epoll
// ═══════════════════════════════════════════════════════════════════════════
#ifndef _WIN32

class EpollPoller {
public:
    static constexpr int kDefaultMaxEvents = 64;

    explicit EpollPoller(int maxEvents = kDefaultMaxEvents)
        : maxEvents_(maxEvents), rawEvents_(maxEvents)
    {
        epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epollFd_ == -1)
            throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }

    ~EpollPoller() { if (epollFd_ != -1) ::close(epollFd_); }
    EpollPoller(const EpollPoller&)            = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    void add(int fd, uint32_t /*events*/ = ReadyEvent::READY_READ) {
        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == -1)
            throw std::system_error(errno, std::generic_category(), "epoll_ctl(ADD)");
    }

    void remove(int fd) {
        ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
    }

    // Returns ready events; valid until next call to wait().
    std::vector<ReadyEvent> wait(int timeoutMs = -1) {
        int n = ::epoll_wait(epollFd_, rawEvents_.data(),
                             static_cast<int>(rawEvents_.size()), timeoutMs);
        if (n == -1) {
            if (errno == EINTR) return {};
            throw std::system_error(errno, std::generic_category(), "epoll_wait");
        }
        std::vector<ReadyEvent> result(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            result[i].fd     = rawEvents_[i].data.fd;
            result[i].events = ReadyEvent::READY_READ;
        }
        return result;
    }

    [[nodiscard]] int fd() const noexcept { return epollFd_; }

private:
    int                      epollFd_{-1};
    int                      maxEvents_;
    std::vector<epoll_event> rawEvents_;
};

// ═══════════════════════════════════════════════════════════════════════════
// Windows path — select()
// ═══════════════════════════════════════════════════════════════════════════
#else // _WIN32

class EpollPoller {
public:
    explicit EpollPoller(int /*maxEvents*/ = 64) {}
    ~EpollPoller() = default;
    EpollPoller(const EpollPoller&)            = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    void add(int fd, uint32_t /*events*/ = ReadyEvent::READY_READ) {
        watchFds_.push_back(fd);
    }

    void remove(int fd) {
        watchFds_.erase(std::remove(watchFds_.begin(), watchFds_.end(), fd),
                        watchFds_.end());
    }

    std::vector<ReadyEvent> wait(int timeoutMs = -1) {
        if (watchFds_.empty()) {
            if (timeoutMs > 0)
                Sleep(static_cast<DWORD>(timeoutMs));
            return {};
        }

        fd_set readSet;
        FD_ZERO(&readSet);
        for (int fd : watchFds_)
            FD_SET(static_cast<SOCKET>(fd), &readSet);

        timeval tv{};
        timeval* tvp = nullptr;
        if (timeoutMs >= 0) {
            tv.tv_sec  = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            tvp = &tv;
        }

        // select() on Windows ignores the first argument
        int n = ::select(0, &readSet, nullptr, nullptr, tvp);
        if (n == SOCKET_ERROR) return {};
        if (n == 0)            return {};

        std::vector<ReadyEvent> result;
        for (int fd : watchFds_) {
            if (FD_ISSET(static_cast<SOCKET>(fd), &readSet))
                result.push_back({fd, ReadyEvent::READY_READ});
        }
        return result;
    }

    // Windows select() has no kernel fd; return -1 to signal "not applicable"
    [[nodiscard]] int fd() const noexcept { return -1; }

private:
    std::vector<int> watchFds_;
};

#endif // _WIN32

} // namespace netforge
