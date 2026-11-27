# ── Stage 1: Build ───────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel

# ── Stage 2: Runtime ─────────────────────────────────────────────────────────
FROM ubuntu:24.04

WORKDIR /app
COPY --from=builder /src/build/netforge .

EXPOSE 8080

ENTRYPOINT ["./netforge"]
CMD ["--port", "8080", "--workers", "4"]
