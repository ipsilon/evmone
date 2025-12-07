# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(evmone-bench)
target_include_directories(evmone-bench PRIVATE ${evmone_private_include_dir})
target_link_libraries(evmone-bench PRIVATE evmone evmone::testutils evmc::loader benchmark::benchmark)
target_sources(
    evmone-bench PRIVATE
    bench.cpp
    helpers.hpp
    synthetic_benchmarks.cpp synthetic_benchmarks.hpp
)

# Tests

set(PREFIX evmone/bench)
set(BENCHMARK_SUITE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../evm-benchmarks/benchmarks)

# Check if DIR argument works.
add_test(NAME ${PREFIX}/dir COMMAND evmone-bench ${BENCHMARK_SUITE_DIR} --benchmark_list_tests)
set_tests_properties(${PREFIX}/dir PROPERTIES PASS_REGULAR_EXPRESSION "total/synth")

# Omitting DIR is fine.
add_test(NAME ${PREFIX}/no_dir COMMAND evmone-bench --benchmark_list_tests)
set_tests_properties(${PREFIX}/no_dir PROPERTIES PASS_REGULAR_EXPRESSION "total/synth")

# Empty DIR name should list only built-in benchmarks.
add_test(NAME ${PREFIX}/dirname_empty COMMAND evmone-bench "" --benchmark_list_tests)
set_tests_properties(${PREFIX}/dirname_empty PROPERTIES PASS_REGULAR_EXPRESSION "total/synth")

# Run all benchmark cases split into groups to check if none of them crashes.
add_test(NAME ${PREFIX}/synth COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=synth)
add_test(NAME ${PREFIX}/micro COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=micro ${BENCHMARK_SUITE_DIR})
add_test(NAME ${PREFIX}/main/b COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=main/[b] ${BENCHMARK_SUITE_DIR})
add_test(NAME ${PREFIX}/main/s COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=main/[s] ${BENCHMARK_SUITE_DIR})
add_test(NAME ${PREFIX}/main/w COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=main/[w] ${BENCHMARK_SUITE_DIR})
add_test(NAME ${PREFIX}/main/_ COMMAND evmone-bench --benchmark_min_time=1x --benchmark_filter=main/[^bsw] ${BENCHMARK_SUITE_DIR})
