/**
 * NetForge — bench_throughput.cpp  (cross-platform)
 *
 * Usage:
 *   ./netforge_bench [--host ip] [--port n] [--clients n] [--duration s] [--warmup s]
 */

#include "netforge/Platform.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

struct BenchConfig {
    std::string host{"127.0.0.1"};
    uint16_t    port{8080};
    int         clients{10};
    int         durationSec{10};
    int         warmupSec{2};
};

struct ClientResult {
    uint64_t             requests{0};
    std::vector<int64_t> latenciesUs;
};

static int connectTo(const std::string& host, uint16_t port) {
#ifdef _WIN32
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) return -1;
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
#ifdef _WIN32
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(s); return -1;
    }
    return static_cast<int>(s);
#else
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        ::close(s); return -1;
    }
    return s;
#endif
}

static ClientResult runClient(const BenchConfig& cfg,
                               std::atomic<bool>& warmupDone,
                               std::atomic<bool>& stop)
{
    ClientResult res;
    int fd = connectTo(cfg.host, cfg.port);
    if (fd < 0) return res;

    const char kPing[] = "PING\n";
    char buf[64];
    bool measuring = false;

    while (!stop.load(std::memory_order_relaxed)) {
        if (!measuring && warmupDone.load(std::memory_order_relaxed))
            measuring = true;

        auto t0 = std::chrono::steady_clock::now();
#ifdef _WIN32
        ::send(static_cast<SOCKET>(fd), kPing, (int)(sizeof(kPing)-1), 0);
        int n = ::recv(static_cast<SOCKET>(fd), buf, sizeof(buf), 0);
#else
        ::send(fd, kPing, sizeof(kPing)-1, MSG_NOSIGNAL);
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        auto t1 = std::chrono::steady_clock::now();

        if (measuring) {
            ++res.requests;
            res.latenciesUs.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count());
        }
    }

    const char kQuit[] = "QUIT\n";
#ifdef _WIN32
    ::send(static_cast<SOCKET>(fd), kQuit, (int)(sizeof(kQuit)-1), 0);
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::send(fd, kQuit, sizeof(kQuit)-1, MSG_NOSIGNAL);
    ::close(fd);
#endif
    return res;
}

static int64_t percentile(std::vector<int64_t>& v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(p / 100.0 * (v.size()-1))];
}

int main(int argc, char* argv[]) {
    netforge::platformInit();
    BenchConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a=="--host"     && i+1<argc) cfg.host        = argv[++i];
        else if (a=="--port"     && i+1<argc) cfg.port        = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (a=="--clients"  && i+1<argc) cfg.clients     = std::stoi(argv[++i]);
        else if (a=="--duration" && i+1<argc) cfg.durationSec = std::stoi(argv[++i]);
        else if (a=="--warmup"   && i+1<argc) cfg.warmupSec   = std::stoi(argv[++i]);
    }

    std::atomic<bool> warmupDone{false}, stop{false};
    std::vector<std::thread>  threads;
    std::vector<ClientResult> results(cfg.clients);

    std::cout << "Connecting " << cfg.clients << " clients to "
              << cfg.host << ':' << cfg.port << " ...\n";

    for (int c = 0; c < cfg.clients; ++c)
        threads.emplace_back([&,c]{ results[c] = runClient(cfg, warmupDone, stop); });

    std::cout << "Warming up for " << cfg.warmupSec << "s ...\n";
    std::this_thread::sleep_for(std::chrono::seconds(cfg.warmupSec));
    warmupDone = true;

    std::cout << "Measuring for " << cfg.durationSec << "s ...\n";
    std::this_thread::sleep_for(std::chrono::seconds(cfg.durationSec));
    stop = true;

    for (auto& t : threads) t.join();

    uint64_t totalReqs = 0;
    std::vector<int64_t> all;
    for (auto& r : results) {
        totalReqs += r.requests;
        all.insert(all.end(), r.latenciesUs.begin(), r.latenciesUs.end());
    }

    double rps = static_cast<double>(totalReqs) / cfg.durationSec;
    std::cout << "\n"
              << "════════════════════ Results ════════════════════\n"
              << "  Clients    : " << cfg.clients << '\n'
              << "  Duration   : " << cfg.durationSec << "s\n"
              << "  Total reqs : " << totalReqs << '\n'
              << std::fixed << std::setprecision(0)
              << "  Throughput : " << rps << " req/s\n"
              << "  p50 latency: " << percentile(all, 50) << " us\n"
              << "  p90 latency: " << percentile(all, 90) << " us\n"
              << "  p99 latency: " << percentile(all, 99) << " us\n"
              << "═════════════════════════════════════════════════\n";

    netforge::platformCleanup();
    return 0;
}
