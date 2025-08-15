# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2024 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(evmone-precompiles-bench)
target_compile_features(evmone-precompiles-bench PRIVATE cxx_std_20)
target_include_directories(evmone-precompiles-bench PRIVATE ..)
target_link_libraries(evmone-precompiles-bench PRIVATE evmone::state benchmark::benchmark)
target_sources(
    evmone-precompiles-bench PRIVATE
    bls_bench_input.hpp
    bls_bench_input.cpp
    precompiles_bench.cpp
)

add_test(NAME evmone/evmone-precompiles-bench COMMAND evmone-precompiles-bench --benchmark_min_time=1x)
set_tests_properties(evmone/evmone-precompiles-bench PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR")
