cmake_minimum_required(VERSION 3.20)

# 测试框架 (简单实现，不依赖外部库)
add_executable(bindiff_tests
    test_main.cpp
    test_mmap.cpp
    test_matcher.cpp
    test_operations.cpp
    test_compress.cpp
)

target_link_libraries(bindiff_tests PRIVATE bindiff_lib)

# 添加测试
add_test(NAME bindiff_tests COMMAND bindiff_tests)

# 基准测试 (可选)
if(BINDIFF_BUILD_BENCHMARKS)
    add_executable(bindiff_bench
        benchmark/bench_large.cpp
    )
    target_link_libraries(bindiff_bench PRIVATE bindiff_lib)
endif()
