# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(
    evmone-bench-internal
    modarith_bench.cpp
    find_jumpdest_bench.cpp
    jumpdest_analysis_bench.cpp
    test_bytecodes.cpp
    memory_allocation.cpp
    lru_cache_bench.cpp
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64" AND NOT APPLE AND NOT WIN32)
    enable_language(ASM)
    target_sources(evmone-bench-internal PRIVATE jumpdest/frozen_asm.S
        jumpdest/r3/g9np_avx2_ofix.S jumpdest/r3/g9np_avx2_o41.S jumpdest/r3/c8_avx2_fnew1.S)
    target_compile_definitions(evmone-bench-internal PRIVATE JDA_FROZEN_ASM=1)
endif()

# TODO: Not sure why evmone::precompiles must be here, but otherwise this don't link.
target_link_libraries(evmone-bench-internal PRIVATE evmone evmone::precompiles benchmark::benchmark)
target_include_directories(evmone-bench-internal PRIVATE ${evmone_private_include_dir})
