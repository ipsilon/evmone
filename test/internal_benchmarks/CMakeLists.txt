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

add_executable(
    evmone-bench-jumpdest-pointer
    jumpdest_pointer_bench.cpp
    jumpdest_pointer_mode_def.cpp
    jumpdest_pointer_mode_al.cpp
    jumpdest_pointer_mode_alj.cpp
    test_bytecodes.cpp
)
target_link_libraries(evmone-bench-jumpdest-pointer PRIVATE evmc::evmc_cpp benchmark::benchmark_main)
target_include_directories(evmone-bench-jumpdest-pointer PRIVATE ${evmone_private_include_dir})

# The code layout flags of the modes. A mode the compiler cannot build gets no JDP_FLAGS and its
# benchmarks are skipped.
include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(jdp_jcc_flag -Wa,-mbranches-within-32B-boundaries)
else()
    set(jdp_jcc_flag -mbranches-within-32B-boundaries)
endif()
check_cxx_compiler_flag(-falign-loops=64 jdp_have_align_loops)
check_cxx_compiler_flag(${jdp_jcc_flag} jdp_have_jcc)
set_source_files_properties(jumpdest_pointer_mode_def.cpp PROPERTIES COMPILE_DEFINITIONS JDP_FLAGS=default)
if(jdp_have_align_loops)
    set_source_files_properties(
        jumpdest_pointer_mode_al.cpp PROPERTIES
        COMPILE_OPTIONS -falign-loops=64
        COMPILE_DEFINITIONS JDP_FLAGS=-falign-loops=64
    )
    if(jdp_have_jcc)
        set_source_files_properties(
            jumpdest_pointer_mode_alj.cpp PROPERTIES
            COMPILE_OPTIONS "-falign-loops=64;${jdp_jcc_flag}"
            COMPILE_DEFINITIONS "JDP_FLAGS=-falign-loops=64 ${jdp_jcc_flag}"
        )
    endif()
endif()
