# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(
    evmone-bench-internal
    modarith_bench.cpp
    find_jumpdest_bench.cpp
    memory_allocation.cpp
    lru_cache_bench.cpp
)

# TODO: Not sure why evmone::precompiles must be here, but otherwise this don't link.
target_link_libraries(evmone-bench-internal PRIVATE evmone evmone::precompiles benchmark::benchmark)
target_include_directories(evmone-bench-internal PRIVATE ${evmone_private_include_dir})

add_executable(evmone-bench-jumpdest-pointer jumpdest_pointer_bench.cpp test_bytecodes.cpp)
target_link_libraries(evmone-bench-jumpdest-pointer PRIVATE evmc::evmc_cpp benchmark::benchmark_main)
target_include_directories(evmone-bench-jumpdest-pointer PRIVATE ${evmone_private_include_dir})
