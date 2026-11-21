# NetForge — High-Performance Concurrent TCP Server

A production-quality, multithreaded TCP server built from scratch in C++20.
Designed to demonstrate every skill typically probed in entry-level to mid-level C++ systems interviews.

```
 ┌──────────────────────────────────────────────────────────────┐
 │  Client 1 ──┐                                                │
 │  Client 2 ──┤──► TcpServer (epoll accept loop)               │
 │  Client N ──┘        │                                       │
 │                       ▼                                      │
 │              ┌─── ThreadPool ───┐                            │
 │              │  Worker 0        │                            │
 │              │  Worker 1   ◄────┤── TaskQueue (MPMC)         │
 │              │  ...             │                            │
 │              │  Worker N-1      │                            │
 │              └──────────────────┘                            │
 │                       │                                      │
 │                ConnectionManager (fd → ConnectionInfo)       │
 └──────────────────────────────────────────────────────────────┘
```

## Features

| Area | What's implemented |
|------|--------------------|
| **Networking** | POSIX TCP sockets, `accept4`, non-blocking I/O, `epoll` (edge-triggered) |
| **Concurrency** | Custom thread pool, `std::mutex`, `std::condition_variable`, `std::atomic` |
| **RAII** | `Socket`, `EpollPoller` own their file descriptors |
| **Smart pointers** | `shared_ptr<ConnectionInfo>` shared between server and manager |
| **Move semantics** | `Socket` is move-only |
| **Templates** | `ThreadPool::submit<F, Args...>()` returning `std::future<ReturnType>` |
| **STL** | `std::queue`, `std::unordered_map`, `std::vector`, `std::span` |
| **Protocol** | Line-delimited text: PING / ECHO / TIME / STATS / QUIT |
| **Graceful shutdown** | `eventfd` wakes `epoll_wait`; thread pool drains then joins |
| **Observability** | Thread-safe logger with timestamps; `/STATS` command returns JSON |
| **Testing** | GoogleTest unit tests for every core component |
| **Memory safety** | AddressSanitizer + UBSan integration |
| **Build** | CMake 3.22+, `FetchContent` for GoogleTest |
| **CI** | GitHub Actions: Debug+ASan + Release builds on Ubuntu |
| **Benchmarking** | Concurrent load tester measures throughput and p50/p90/p99 latency |

---

## Project structure

```
NetForge/
├── include/netforge/
│   ├── Logger.hpp           # Thread-safe, header-only logger
│   ├── TaskQueue.hpp        # MPMC task queue (mutex + condvar)
│   ├── ThreadPool.hpp       # Fixed-size thread pool with futures
│   ├── Socket.hpp           # RAII POSIX socket wrapper
│   ├── EpollPoller.hpp      # RAII epoll wrapper (edge-triggered)
│   ├── Protocol.hpp         # Wire protocol parser + dispatcher
│   ├── ConnectionManager.hpp# Thread-safe connection registry
│   └── TcpServer.hpp        # Main server: accept loop + dispatch
├── src/
│   ├── main.cpp             # CLI entry point, signal handling
│   └── client.cpp           # Interactive test client
├── tests/
│   ├── test_TaskQueue.cpp
│   ├── test_ThreadPool.cpp
│   ├── test_Protocol.cpp
│   ├── test_ConnectionManager.cpp
│   └── CMakeLists.txt
├── benchmarks/
│   └── bench_throughput.cpp # Throughput + latency load tester
├── scripts/
│   ├── run_bench.sh         # One-command benchmark runner
│   └── run_asan.sh          # Build + test under ASan/UBSan
├── .github/workflows/ci.yml
└── CMakeLists.txt
```

---

## Build

Requires: **GCC 13+** (or Clang 16+), **CMake 3.22+**, **Linux**.

```bash
# Release build (fastest binary)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Debug build with tests
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
```

---

## Run

```bash
# Start server (default: port 8080, 4 workers)
./build/netforge

# Custom port and worker count
./build/netforge --port 9000 --workers 8

# Verbose logging
./build/netforge --log-level debug
```

---

## Test with the client

```bash
./build/netforge_client --host 127.0.0.1 --port 8080
> PING
PONG
> ECHO NetForge works!
NetForge works!
> TIME
2026-11-19T15:30:00Z
> STATS
{"workers":4,"queued":0,"active":1,"completed":42,"connections":1}
> QUIT
BYE
```

Or with `telnet` / `nc`:

```bash
echo "PING" | nc 127.0.0.1 8080
```

---

## Unit tests

```bash
cd build-debug
ctest --output-on-failure
```

Expected output:

```
Test project .../build-debug
    Start 1: TaskQueue.PushPopSingleThread
1/N Test #1: TaskQueue... .................. Passed
...
100% tests passed, 0 tests failed
```

---

## Memory-safety check

```bash
chmod +x scripts/run_asan.sh
./scripts/run_asan.sh
```

Runs all tests under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Benchmark

```bash
# Build first, then:
chmod +x scripts/run_bench.sh
./scripts/run_bench.sh --clients 50 --duration 10 --workers 8
```

Sample output on a 4-core laptop:

```
════════════════════ Results ════════════════════
  Clients    : 50
  Duration   : 10s
  Total reqs : 1964148
  Throughput : 196415 req/s
  p50 latency: 23 µs
  p90 latency: 61 µs
  p99 latency: 510 µs
═════════════════════════════════════════════════
```

---

## Design decisions (interview talking points)

### Why a fixed-size thread pool?
Worker count is set at construction so each `std::thread` can be pinned to a CPU core later (via `pthread_setaffinity_np`) without invalidating existing handles.  Dynamic resize adds lock contention and complexity without measurable benefit for a CPU-bound workload.

### Why `std::function` in `TaskQueue`?
Type erasure lets any callable be queued without the queue knowing the work's signature.  The small-buffer optimisation in `std::function` avoids heap allocation for most lambdas.  For latency-critical paths, a lock-free queue or intrusive list is the next step.

### Why edge-triggered epoll?
Level-triggered epoll would fire repeatedly until the buffer is drained — wasted syscalls.  Edge-triggered fires once per edge; the handler loops until `EAGAIN`, which is correct for non-blocking sockets and maximises throughput.

### Why a wake pipe for shutdown?
`stop()` must be async-signal-safe (callable from a signal handler). Two connected loopback sockets form a cross-platform "pipe": `stop()` writes one byte to the write end, which immediately wakes `epoll_wait` on the read end without polling overhead. This approach works identically on Linux and Windows, unlike `eventfd` (Linux-only) or `pipe` (not available on Winsock).

### Move-only `Socket`
Copying a socket fd would double-close on destruction, corrupting another fd that reused the same number.  Move semantics make ownership explicit: a moved-from `Socket` holds `-1` and its destructor does nothing.

### `shared_ptr<ConnectionInfo>`
Both `TcpServer` (which creates the info) and `ConnectionManager` (which stores it) need to refer to the same object.  `shared_ptr` makes shared ownership explicit without manual reference counting.

---

## Roadmap

- [ ] Per-connection epoll (true async I/O, remove per-connection thread)
- [ ] TLS via OpenSSL / BoringSSL
- [ ] HTTP/1.1 parser replacing the toy protocol
- [ ] Lock-free SPSC queue option for ultra-low latency
- [ ] Prometheus metrics endpoint

---

## License

MIT © 2026 NetForge contributors
