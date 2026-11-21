#!/usr/bin/env bash
# NetForge — run_bench.sh
# Starts the server, waits for it, runs the benchmark, then stops the server.
#
# Usage:
#   ./scripts/run_bench.sh [--clients N] [--duration S] [--workers W]

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
PORT=8080
WORKERS=8
CLIENTS=50
DURATION=10
WARMUP=2

while [[ $# -gt 0 ]]; do
    case $1 in
        --clients)  CLIENTS=$2;  shift 2 ;;
        --duration) DURATION=$2; shift 2 ;;
        --workers)  WORKERS=$2;  shift 2 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

SERVER="${BUILD_DIR}/netforge"
BENCH="${BUILD_DIR}/netforge_bench"

[[ -x "$SERVER" ]] || { echo "Build the project first: cmake --build ${BUILD_DIR}"; exit 1; }
[[ -x "$BENCH"  ]] || { echo "Benchmark binary not found in ${BUILD_DIR}"; exit 1; }

echo "Starting NetForge on port ${PORT} with ${WORKERS} workers ..."
"$SERVER" --port "$PORT" --workers "$WORKERS" --log-level warn &
SERVER_PID=$!

cleanup() { kill "$SERVER_PID" 2>/dev/null || true; }
trap cleanup EXIT

# Wait until the server is accepting connections.
for i in $(seq 1 20); do
    if bash -c "echo PING | nc -q1 127.0.0.1 ${PORT}" &>/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

echo "Running benchmark: ${CLIENTS} clients, ${DURATION}s measurement ..."
"$BENCH" \
    --host 127.0.0.1 \
    --port "$PORT"   \
    --clients  "$CLIENTS"  \
    --duration "$DURATION" \
    --warmup   "$WARMUP"

echo "Benchmark complete."
