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

# Register execution-spec-test (EEST) fixtures as individual ctest tests: one per
# fixture file, so they run in parallel under `ctest --parallel` instead of a single
# serial runner invocation. EVMONE_SPEC_TESTS is a list of base directories scanned
# recursively; every *.json file below a `state_tests` or `blockchain_tests`
# directory becomes a test that runs the matching runner on that single file. The
# test name is the file path relative to the base directory (e.g.
# execution-specs/fixtures/state_tests/...), so fixtures from different sources stay
# distinct. The fixtures must exist at configure time. Select a type with e.g.
# `ctest -L state_tests`; select a source with `ctest -R <base-subdir>`.
set(EVMONE_SPEC_TESTS "" CACHE STRING
    "List of base directories scanned for execution-spec-test fixtures to register as ctest tests")

foreach(base ${EVMONE_SPEC_TESTS})
    file(GLOB_RECURSE spec_test_files "${base}/*.json")
    foreach(file ${spec_test_files})
        get_filename_component(fname "${file}" NAME)
        if(fname STREQUAL "index.json")  # Index files list other tests, not tests themselves.
            continue()
        endif()
        file(RELATIVE_PATH name "${base}" "${file}")
        # The fixture type is the directory right below `fixtures/`. Matching any
        # `state_tests/` in the path instead would misclassify the engine fixtures,
        # which carry a nested state_tests/ subdirectory of imported static tests.
        # Unknown types (blockchain_tests_engine*, _sync, transaction_tests, ...)
        # are not run here.
        if(NOT "${name}" MATCHES "(^|/)fixtures/([^/]+)/")
            continue()
        endif()
        set(type ${CMAKE_MATCH_2})
        if(type STREQUAL "state_tests")
            set(runner evmone-statetest)
        elseif(type STREQUAL "blockchain_tests")
            set(runner evmone-blockchaintest)
        else()
            continue()
        endif()
        add_test(NAME "${name}" COMMAND ${runner} "${file}")
        # Per-process profraw so parallel coverage runs do not clobber each other.
        set_tests_properties("${name}" PROPERTIES LABELS "${type}"
            ENVIRONMENT "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/${type}-%m.profraw")
    endforeach()
endforeach()
