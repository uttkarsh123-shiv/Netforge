cmake_minimum_required(VERSION 3.22)
project(NetForge VERSION 1.0.0 LANGUAGES CXX)

# ── C++20 ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── Platform link libraries ───────────────────────────────────────────────────
# ws2_32 = Winsock2, mswsock = extension functions — required on Windows.
set(PLATFORM_LIBS "")
if(WIN32)
    list(APPEND PLATFORM_LIBS ws2_32 mswsock)
endif()

# ── Header-only core library ──────────────────────────────────────────────────
add_library(netforge_lib INTERFACE)
target_include_directories(netforge_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(netforge_lib INTERFACE ${PLATFORM_LIBS})

# ── Server ────────────────────────────────────────────────────────────────────
add_executable(netforge src/main.cpp)
target_link_libraries(netforge PRIVATE netforge_lib)

# ── Client ───────────────────────────────────────────────────────────────────
add_executable(netforge_client src/client.cpp)
target_link_libraries(netforge_client PRIVATE netforge_lib)

# ── Benchmark ─────────────────────────────────────────────────────────────────
add_executable(netforge_bench benchmarks/bench_throughput.cpp)
target_link_libraries(netforge_bench PRIVATE netforge_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
option(NETFORGE_BUILD_TESTS "Build unit tests" ON)
if(NETFORGE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS netforge netforge_client netforge_bench RUNTIME DESTINATION bin)

# ── Summary ───────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "═══════════ NetForge Build Configuration ═══════════")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform     : ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Tests        : ${NETFORGE_BUILD_TESTS}")
message(STATUS "═════════════════════════════════════════════════════")
message(STATUS "")
