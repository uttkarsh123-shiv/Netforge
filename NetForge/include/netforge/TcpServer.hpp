#pragma once
/**
 * NetForge — TcpServer.hpp  (cross-platform: Windows + Linux)
 *
 * Accept loop uses a self-pipe trick for graceful shutdown:
 *  - Two connected sockets form a "wake pipe"; stop() writes one byte to
 *    the write end; the poller sees the read end as readable and exits.
 *  - On Linux, eventfd was replaced with this approach so the same code
 *    compiles on Windows (Winsock) and Linux (POSIX).
 *
 * Everything else is identical to the original design:
 *   TcpServer srv(config);
 *   srv.run();    // blocks until stop()
 *   srv.stop();   // safe from any thread
 */

#include "ConnectionManager.hpp"
#include "EpollPoller.hpp"
#include "Logger.hpp"
#include "Platform.hpp"
#include "Protocol.hpp"
#include "Socket.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace netforge {

struct ServerConfig {
    uint16_t    port{8080};
    std::size_t workerThreads{4};
    int         backlog{128};
    std::size_t recvBufSize{4096};
};

class TcpServer {
public:
    explicit TcpServer(ServerConfig cfg)
        : cfg_(cfg)
        , pool_(cfg.workerThreads)
    {
        setupListenSocket();
        setupWakePipe();
    }

    ~TcpServer() {
        closeSocket(wakePipeRead_);
        closeSocket(wakePipeWrite_);
    }

    void run() {
        NF_INFO("NetForge listening on port ", cfg_.port,
                " with ", cfg_.workerThreads, " workers");

        EpollPoller poller;
        poller.add(listenSock_);
        poller.add(wakePipeRead_);

        running_ = true;
        while (running_) {
            auto events = poller.wait(500);
            for (auto& ev : events) {
                if (ev.fd == wakePipeRead_) {
                    running_ = false;
                    break;
                }
                if (ev.fd == static_cast<int>(listenSock_)) {
                    acceptConnections();
                }
            }
        }
        NF_INFO("Server accept loop exiting. Active connections: ",
                manager_.count());
    }

    void stop() {
        // Write one byte to the wake pipe to unblock the poller.
        const char byte = 1;
#ifdef _WIN32
        ::send(static_cast<SOCKET>(wakePipeWrite_), &byte, 1, 0);
#else
        if (::write(wakePipeWrite_, &byte, 1) == -1) {
            // Best-effort wake; if this fails the poll timeout will still exit.
        }
#endif
    }

    [[nodiscard]] std::string statsJson() const {
        auto s     = pool_.stats();
        auto conns = manager_.count();
        std::ostringstream oss;
        oss << "{"
            << "\"workers\":"    << s.workerCount   << ","
            << "\"queued\":"     << s.queuedTasks    << ","
            << "\"active\":"     << s.activeTasks    << ","
            << "\"completed\":"  << s.completedTasks << ","
            << "\"connections\":" << conns
            << "}";
        return oss.str();
    }

private:
    // ── Setup ────────────────────────────────────────────────────────────────

    void setupListenSocket() {
        listenSock_ = Socket::makeTCP();
        listenSock_.setReuseAddr();
        listenSock_.setReusePort();
        listenSock_.setNonBlocking();

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(cfg_.port);

#ifdef _WIN32
        if (::bind(static_cast<SOCKET>(static_cast<int>(listenSock_)),
                   reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            throw std::runtime_error("bind failed: " + lastNetError());
        if (::listen(static_cast<SOCKET>(static_cast<int>(listenSock_)),
                     cfg_.backlog) == SOCKET_ERROR)
            throw std::runtime_error("listen failed: " + lastNetError());
#else
        if (::bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
            throw std::system_error(errno, std::generic_category(), "bind");
        if (::listen(listenSock_, cfg_.backlog) == -1)
            throw std::system_error(errno, std::generic_category(), "listen");
#endif
    }

    void setupWakePipe() {
        // Create a loopback TCP pair that acts like a pipe.
        // This works on both Windows and Linux without platform-specific APIs.
        Socket listener = Socket::makeTCP();
        listener.setReuseAddr();

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0;  // OS picks port

#ifdef _WIN32
        SOCKET ls = static_cast<SOCKET>(static_cast<int>(listener));
        if (::bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            throw std::runtime_error("wake pipe bind: " + lastNetError());
        if (::listen(ls, 1) == SOCKET_ERROR)
            throw std::runtime_error("wake pipe listen: " + lastNetError());
        int addrLen = sizeof(addr);
        if (::getsockname(ls, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
            throw std::runtime_error("wake pipe getsockname: " + lastNetError());
#else
        if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
            throw std::system_error(errno, std::generic_category(), "wake pipe bind");
        if (::listen(listener, 1) == -1)
            throw std::system_error(errno, std::generic_category(), "wake pipe listen");
        socklen_t addrLen = sizeof(addr);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addrLen) == -1)
            throw std::system_error(errno, std::generic_category(), "wake pipe getsockname");
#endif

        // Write end: connect to the loopback listener
        Socket writer = Socket::makeTCP();
#ifdef _WIN32
        SOCKET ws = static_cast<SOCKET>(static_cast<int>(writer));
        ::connect(ws, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        SOCKET rs = ::accept(ls, nullptr, nullptr);
        if (rs == INVALID_SOCKET)
            throw std::runtime_error("wake pipe accept: " + lastNetError());
        wakePipeRead_  = static_cast<int>(rs);
        wakePipeWrite_ = writer.release();
#else
        ::connect(writer, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        int rs = ::accept(listener, nullptr, nullptr);
        if (rs == -1)
            throw std::system_error(errno, std::generic_category(), "wake pipe accept");
        wakePipeRead_  = rs;
        wakePipeWrite_ = writer.release();
#endif
    }

    // ── Accept loop ──────────────────────────────────────────────────────────

    void acceptConnections() {
        while (true) {
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = ::accept(
                static_cast<SOCKET>(static_cast<int>(listenSock_)),
                reinterpret_cast<sockaddr*>(&clientAddr),
                &addrLen);
            if (clientSock == INVALID_SOCKET) break;
            int clientFd = static_cast<int>(clientSock);
#else
            socklen_t addrLen = sizeof(clientAddr);
            int clientFd = ::accept4(listenSock_,
                                     reinterpret_cast<sockaddr*>(&clientAddr),
                                     &addrLen,
                                     SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (clientFd == -1) break;
#endif
            char ipBuf[INET_ADDRSTRLEN]{};
            ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));

            auto info = std::make_shared<ConnectionInfo>(
                clientFd, ipBuf, ntohs(clientAddr.sin_port)
            );
            manager_.add(info);

            NF_INFO("Accepted [", clientFd, "] from ",
                    info->remoteAddr, ':', info->remotePort);

            pool_.submit([this, clientFd, info = std::move(info)]() mutable {
                handleConnection(clientFd, std::move(info));
            });
        }
    }

    // ── Connection handler (runs on thread pool) ─────────────────────────────

    void handleConnection(int fd, std::shared_ptr<ConnectionInfo> info) {
        Socket sock{fd};
        sock.setTcpNoDelay();

        std::string recvBuf;
        recvBuf.reserve(cfg_.recvBufSize);
        char chunk[1024];

        while (true) {
            ssize_t n = sock.recv(chunk, sizeof(chunk));
            if (n == 0) {
                NF_INFO("Client [", fd, "] closed connection");
                break;
            }
            if (n < 0) {
                int err = netErrno();
                if (err == EAGAIN || err == EWOULDBLOCK) continue;
                NF_WARN("recv error on [", fd, "]: ", lastNetError());
                break;
            }

            recvBuf.append(chunk, static_cast<std::size_t>(n));

            std::size_t pos = 0;
            while (true) {
                auto nl = recvBuf.find('\n', pos);
                if (nl == std::string::npos) break;

                std::string_view line(recvBuf.data() + pos, nl - pos + 1);
                auto req  = protocol::parse(line);
                auto resp = protocol::dispatch(req, statsJson());

                info->requestsHandled.fetch_add(1, std::memory_order_relaxed);

                if (!sock.sendAll(resp.body.data(), resp.body.size())) {
                    NF_WARN("send failed on [", fd, "]");
                    goto cleanup;
                }
                if (resp.closeAfter) goto cleanup;

                pos = nl + 1;
            }
            recvBuf.erase(0, pos);
        }

    cleanup:
        NF_INFO("Closing [", fd, "]. Requests handled: ",
                info->requestsHandled.load());
        manager_.remove(fd);
    }

    // ── Members ──────────────────────────────────────────────────────────────

    ServerConfig      cfg_;
    Socket            listenSock_;
    int               wakePipeRead_{-1};
    int               wakePipeWrite_{-1};
    ThreadPool        pool_;
    ConnectionManager manager_;
    std::atomic<bool> running_{false};
};

} // namespace netforge
