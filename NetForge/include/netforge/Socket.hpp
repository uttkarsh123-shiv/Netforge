#pragma once
/**
 * NetForge — Socket.hpp
 *
 * RAII wrapper around a socket descriptor.
 * Compiles on both Windows (Winsock2) and Linux (POSIX).
 *
 * Design decisions:
 *  - Move-only: ownership transfers on move, copy is deleted.
 *  - Destructor calls closeSocket() — no manual fd management required.
 *  - setNonBlocking() wraps ioctlsocket/fcntl depending on platform.
 *  - operator int() allows passing directly to send/recv/select.
 */

#include "Platform.hpp"

#include <stdexcept>
#include <string>
#include <system_error>
#include <cstring>

namespace netforge {

class Socket {
public:
    static constexpr int kInvalid = -1;

    explicit Socket(int fd = kInvalid) noexcept : fd_(fd) {}

    /// Factory: creates a TCP/IPv4 non-blocking socket.
    static Socket makeTCP() {
#ifdef _WIN32
        SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET)
            throw std::runtime_error("socket() failed: " + lastNetError());
        return Socket{static_cast<int>(s)};
#else
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd == kInvalid)
            throw std::system_error(errno, std::generic_category(), "socket()");
        return Socket{fd};
#endif
    }

    ~Socket() { close(); }

    Socket(Socket&& other) noexcept : fd_(other.release()) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) { close(); fd_ = other.release(); }
        return *this;
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] bool valid()   const noexcept { return fd_ != kInvalid; }
    operator int()               const noexcept { return fd_; }

    [[nodiscard]] int release() noexcept {
        int fd = fd_; fd_ = kInvalid; return fd;
    }

    void close() noexcept {
        if (fd_ != kInvalid) { closeSocket(fd_); fd_ = kInvalid; }
    }

    // ── Socket options ───────────────────────────────────────────────────────

    void setReuseAddr(bool on = true) {
        int v = on ? 1 : 0;
#ifdef _WIN32
        if (::setsockopt(static_cast<SOCKET>(fd_), SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<const char*>(&v), sizeof(v)) != 0)
            throw std::runtime_error("SO_REUSEADDR: " + lastNetError());
#else
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) == -1)
            throw std::system_error(errno, std::generic_category(), "SO_REUSEADDR");
#endif
    }

    void setReusePort([[maybe_unused]] bool on = true) {
#ifndef _WIN32
        // SO_REUSEPORT is Linux-only; silently skip on Windows
        int v = on ? 1 : 0;
        if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &v, sizeof(v)) == -1)
            throw std::system_error(errno, std::generic_category(), "SO_REUSEPORT");
#endif
    }

    void setTcpNoDelay(bool on = true) {
        int v = on ? 1 : 0;
#ifdef _WIN32
        if (::setsockopt(static_cast<SOCKET>(fd_), IPPROTO_TCP, TCP_NODELAY,
                         reinterpret_cast<const char*>(&v), sizeof(v)) != 0)
            throw std::runtime_error("TCP_NODELAY: " + lastNetError());
#else
        if (::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) == -1)
            throw std::system_error(errno, std::generic_category(), "TCP_NODELAY");
#endif
    }

    void setNonBlocking(bool on = true) {
#ifdef _WIN32
        u_long mode = on ? 1UL : 0UL;
        if (::ioctlsocket(static_cast<SOCKET>(fd_), FIONBIO, &mode) != 0)
            throw std::runtime_error("ioctlsocket(FIONBIO): " + lastNetError());
#else
        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1)
            throw std::system_error(errno, std::generic_category(), "fcntl(F_GETFL)");
        flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        if (::fcntl(fd_, F_SETFL, flags) == -1)
            throw std::system_error(errno, std::generic_category(), "fcntl(F_SETFL)");
#endif
    }

    // ── I/O helpers ──────────────────────────────────────────────────────────

    bool sendAll(const char* buf, std::size_t len) {
        std::size_t sent = 0;
        while (sent < len) {
#ifdef _WIN32
            int n = ::send(static_cast<SOCKET>(fd_),
                           buf + sent,
                           static_cast<int>(len - sent),
                           0);
            if (n == SOCKET_ERROR) return false;
#else
            ssize_t n = ::send(fd_, buf + sent, len - sent, MSG_NOSIGNAL);
            if (n <= 0) return false;
#endif
            sent += static_cast<std::size_t>(n);
        }
        return true;
    }

    ssize_t recv(char* buf, std::size_t len) {
#ifdef _WIN32
        return static_cast<ssize_t>(
            ::recv(static_cast<SOCKET>(fd_), buf, static_cast<int>(len), 0));
#else
        return ::recv(fd_, buf, len, 0);
#endif
    }

private:
    int fd_;
};

} // namespace netforge
