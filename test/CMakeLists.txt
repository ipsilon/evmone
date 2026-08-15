# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(FetchContent)
include(GoogleTest)

set(evmone_private_include_dir ${PROJECT_SOURCE_DIR}/lib)

hunter_add_package(CLI11)
find_package(CLI11 CONFIG REQUIRED)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

block(PROPAGATE benchmark_libs)
    set(BUILD_SHARED_LIBS OFF)
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(benchmark_libs benchmark benchmark_main)
    if(NOT CODSPEED_MODE)
        FetchContent_Declare(
            benchmark
            URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
            URL_HASH SHA256=9631341c82bac4a288bef951f8b26b41f69021794184ece969f8473977eaa340
        )
    else()
        # Replace google/benchmark with CodSpeed's fork of it to report the benchmark results
        # to https://codspeed.io. It provides the same benchmark::benchmark target,
        # so the benchmark targets need no changes.
        FetchContent_Declare(
            benchmark
            # Fork of v2.4.0 with fixes not yet available upstream.
            GIT_REPOSITORY https://github.com/chfast/codspeed-cpp
            GIT_TAG 0ecdd6e7d8f9e858e8e0b2fbe54ff7f427bec528
            GIT_SHALLOW FALSE
            SOURCE_SUBDIR google_benchmark
        )
        list(APPEND benchmark_libs codspeed instrument_hooks)
    endif()
    FetchContent_MakeAvailable(benchmark)
endblock()

set_target_properties(
    ${benchmark_libs} PROPERTIES
    COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-w>;$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-w>"
    C_CLANG_TIDY ""
    CXX_CLANG_TIDY ""
    SYSTEM TRUE  # TODO(cmake-3.26): Use the SYSTEM option of FetchContent_Declare().
)

include(CableBuildInfo)
cable_add_buildinfo_library(PROJECT_NAME evmone)

add_subdirectory(bench)
add_subdirectory(blockchaintest)
add_subdirectory(experimental)
add_subdirectory(integration)
add_subdirectory(internal_benchmarks)
add_subdirectory(precompiles_bench)
add_subdirectory(statetest)
add_subdirectory(unittests)

set(targets evmone-bench evmone-bench-internal evmone-blockchaintest evmone-precompiles-bench evmone-state evmone-statetest evmone-unittests)

if(EVMONE_FUZZING)
    add_subdirectory(fuzzer)
    list(APPEND targets evmone-fuzzer)
endif()

set_target_properties(
    ${targets} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
)
