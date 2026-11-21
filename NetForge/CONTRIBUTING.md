# Contributing to NetForge

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Running tests

```bash
cd build && ctest --output-on-failure
```

## Running with sanitizers

```bash
chmod +x scripts/run_asan.sh
./scripts/run_asan.sh
```

## Code style

- C++20, no exceptions in hot paths
- RAII for all resources
- Every public class must have a unit test
- No raw `new`/`delete` — use smart pointers

## Commit message format

```
<type>: <short description>

Types: feat | fix | test | bench | ci | docs | refactor
```
