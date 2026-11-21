include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(netforge_tests
    test_TaskQueue.cpp
    test_ThreadPool.cpp
    test_Protocol.cpp
    test_ConnectionManager.cpp
)

target_link_libraries(netforge_tests
    PRIVATE
        netforge_lib
        GTest::gtest_main
)

target_compile_options(netforge_tests PRIVATE -Wall -Wextra)

# AddressSanitizer only on Linux (not supported by MinGW on Windows)
if(NOT WIN32)
    target_compile_options(netforge_tests PRIVATE -fsanitize=address,undefined)
    target_link_options(netforge_tests    PRIVATE -fsanitize=address,undefined)
endif()

include(GoogleTest)
gtest_discover_tests(netforge_tests)
