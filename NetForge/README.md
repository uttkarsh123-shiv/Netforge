<div align="center">

# ⚡ NetForge

### High-Performance Concurrent TCP Server — Built from Scratch in C++20

[![CI](https://github.com/uttkarsh123-shiv/Netforge/actions/workflows/ci.yml/badge.svg)](https://github.com/uttkarsh123-shiv/Netforge/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()

**196,000 req/s · p50 latency 23 µs · 50 concurrent clients**

</div>

---

## What is NetForge?

NetForge is a fully hand-rolled TCP server written in modern C++20 — no Boost, no Asio, no frameworks. It implements a non-blocking epoll accept loop, a custom thread pool with `std::future` support, a lock-aware connection registry, and a line-delimited text protocol, all built on raw POSIX sockets.

It was built to demonstrate how production networking infrastructure actually works under the hood.

---

## Performance

Measured on WSL2 Ubuntu 24.04 (4-core machine):

```
════════════════════ Results ════════════════════
  Clients    : 50
  Duration   : 10s
  Total reqs : 1,964,148
  Throughput : 196,415 req/s
  p50 latency: 23 µs
  p90 latency: 61 µs
  p99 latency: 510 µs
═════════════════════════════════════════════════
```

---

## Architecture

```
  Client 1 ──┐
  Client 2 ──┤──► TcpServer (epoll edge-triggered accept loop)
  Client N ──┘         │
                        ▼
               ┌─── ThreadPool ────┐
               │  Worker 0         │
               │  Worker 1    ◄────┤── TaskQueue (MPMC)
               │  Worker 2         │   mutex + condvar
               │  Worker N-1       │
               └───────────────────┘
                        │
               ConnectionManager
               (fd → shared_ptr<ConnectionInfo>)
```

Each incoming connection is accepted on the epoll loop and immediately dispatched to a thread pool worker. The main thread never blocks on I/O.

---

## Features

| Area | Implementation |
|------|---------------|
| **Networking** | POSIX TCP sockets, `accept4`, non-blocking I/O, edge-triggered `epoll` |
| **Concurrency** | Custom thread pool, `std::mutex`, `std::condition_variable`, `std::atomic` |
| **Memory safety** | RAII `Socket` and `EpollPoller`, move-only ownership, no raw `new`/`delete` |
| **Smart pointers** | `shared_ptr<ConnectionInfo>` shared between server and connection registry |
| **Templates** | `ThreadPool::submit<F, Args...>()` returning `std::future<ReturnType>` |
| **Protocol** | Case-insensitive line-delimited: `PING` `ECHO` `TIME` `STATS` `VERSION` `UPTIME` `CONNECTIONS` `WHOAMI` `REPEAT` `HELP` `QUIT` |
| **Graceful shutdown** | Wake pipe unblocks `epoll_wait`; thread pool drains then joins all workers |
| **Observability** | Thread-safe timestamped logger; `STATS` returns live JSON metrics |
| **Testing** | GoogleTest unit tests for every core component |
| **Memory sanitizers** | AddressSanitizer + UndefinedBehaviorSanitizer integration |
| **Build** | CMake 3.22+, `FetchContent` for GoogleTest, parallel builds |
| **CI** | GitHub Actions: Debug+ASan and Release builds on Ubuntu |
| **Benchmarking** | Concurrent load tester — throughput + p50/p90/p99 latency |

---

## Quickstart

**Requirements:** GCC 13+ or Clang 16+, CMake 3.22+, Linux.

```bash
git clone https://github.com/uttkarsh123-shiv/Netforge.git
cd Netforge

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Start the server
./build/netforge
```

In a second terminal:

```bash
./build/netforge_client
```

```
Connected to 127.0.0.1:8080  (PING / ECHO <msg> / TIME / STATS / VERSION / UPTIME / CONNECTIONS / WHOAMI / REPEAT <n> <msg> / HELP / QUIT)
> PING
PONG
> ECHO NetForge works!
NetForge works!
> TIME
2026-11-21T15:18:33Z
> STATS
{"workers":4,"queued":0,"active":1,"completed":42,"connections":1}
> VERSION
NetForge 1.0.0
> UPTIME
0h 2m 14s
> CONNECTIONS
1
> WHOAMI
127.0.0.1:54321
> REPEAT 3 hello
hello
hello
hello
> HELP
Commands: PING | ECHO <msg> | TIME | STATS | VERSION | UPTIME | CONNECTIONS | WHOAMI | REPEAT <n> <msg> | QUIT
> QUIT
BYE
```

Or with `nc`:

```bash
echo "PING" | nc 127.0.0.1 8080
```

---

## Commands

| Command | Response |
|---------|----------|
| `PING` | `PONG` |
| `ECHO <msg>` | echoes `<msg>` back |
| `TIME` | current UTC time in ISO-8601 format |
| `STATS` | live JSON — workers, queued tasks, active tasks, connections |
| `VERSION` | `NetForge 1.0.0` |
| `UPTIME` | time since server started (e.g. `0h 5m 30s`) |
| `CONNECTIONS` | current active connection count |
| `WHOAMI` | your IP and port (e.g. `127.0.0.1:54321`) |
| `REPEAT <n> <msg>` | prints `<msg>` exactly `<n>` times (max 100) |
| `HELP` | lists all available commands |
| `QUIT` | closes the connection cleanly |

Commands are case-insensitive — `ping`, `Ping`, and `PING` all work.

---

## CLI Options

```bash
./build/netforge --port 9000 --workers 8 --log-level debug
```

| Flag | Default | Description |
|------|---------|-------------|
| `--port` | `8080` | Listening port |
| `--workers` | `4` | Thread pool worker count |
| `--log-level` | `info` | `debug` / `info` / `warn` / `error` |

---

## Running Tests

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
cd build-debug && ctest --output-on-failure
```

---

## Memory Safety

```bash
chmod +x scripts/run_asan.sh && ./scripts/run_asan.sh
```

Builds and runs all tests under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Benchmark

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
chmod +x scripts/run_bench.sh
./scripts/run_bench.sh --clients 50 --duration 10 --workers 8
```

---

## Project Structure

```
NetForge/
├── include/netforge/
│   ├── Platform.hpp          # Cross-platform socket includes and aliases
│   ├── Logger.hpp            # Thread-safe header-only logger
│   ├── TaskQueue.hpp         # MPMC task queue (mutex + condvar)
│   ├── ThreadPool.hpp        # Fixed-size thread pool with std::future
│   ├── Socket.hpp            # Move-only RAII socket wrapper
│   ├── EpollPoller.hpp       # RAII epoll wrapper (edge-triggered)
│   ├── Protocol.hpp          # Protocol parser and dispatcher
│   ├── ConnectionManager.hpp # Thread-safe connection registry
│   └── TcpServer.hpp         # Accept loop, dispatch, graceful shutdown
├── src/
│   ├── main.cpp              # CLI entry point, signal handling
│   └── client.cpp            # Interactive test client
├── tests/                    # GoogleTest unit tests
├── benchmarks/               # Throughput and latency load tester
├── scripts/                  # ASan and benchmark runner scripts
└── .github/workflows/ci.yml  # GitHub Actions CI
```

---

## Design Decisions

### Fixed-size thread pool
Worker count is fixed at construction so each `std::thread` can later be pinned to a CPU core via `pthread_setaffinity_np`. Dynamic resize adds lock contention with no measurable benefit for a CPU-bound workload.

### Edge-triggered epoll
Level-triggered epoll fires repeatedly while data is available — wasted syscalls. Edge-triggered fires once per state change; the handler loops until `EAGAIN`, which is correct for non-blocking sockets and maximises throughput.

### Move-only Socket
Copying an fd would cause a double-close on destruction, corrupting any other fd that reused the same number. A moved-from `Socket` holds `-1` and its destructor does nothing.

### Wake pipe for shutdown
`stop()` is callable from a signal handler and must be async-signal-safe. Two connected loopback sockets form a cross-platform wake pipe: writing one byte to the write end immediately unblocks `epoll_wait` on the read end.

### `shared_ptr<ConnectionInfo>`
Both `TcpServer` and `ConnectionManager` need to refer to the same connection object. `shared_ptr` makes shared ownership explicit without manual reference counting.

---

## Roadmap

- [ ] Per-connection epoll (true async I/O)
- [ ] TLS via OpenSSL / BoringSSL
- [ ] HTTP/1.1 parser
- [ ] Lock-free SPSC queue for ultra-low latency
- [ ] Prometheus metrics endpoint

---

## License

MIT © 2026 [uttkarsh singh](https://github.com/uttkarsh123-shiv)
