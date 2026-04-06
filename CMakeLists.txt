cmake_minimum_required(VERSION 3.14)
project(LimitOrderBook CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent GoogleTest from installing or overriding compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

function(add_test_executable target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE src)
    target_link_libraries(${target} GTest::gtest_main)
    gtest_discover_tests(${target})
endfunction()

add_test_executable(order_tests              tests/test_order.cpp)
add_test_executable(order_book_tests         tests/test_order_book.cpp)
add_test_executable(order_queue_tests        tests/test_order_queue.cpp)
add_test_executable(matching_engine_tests    tests/test_matching_engine.cpp)

add_executable(bench bench/bench.cpp)
target_include_directories(bench PRIVATE src)
target_compile_options(bench PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/O2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2>)
