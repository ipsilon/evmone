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
add_subdirectory(experimental)
add_subdirectory(integration)
add_subdirectory(internal_benchmarks)
add_subdirectory(precompiles_bench)
add_subdirectory(unittests)

set(targets evmone-bench evmone-bench-internal evmone-precompiles-bench evmone-state evmone-unittests)

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

# Register execution-spec-test (EEST) fixtures as ctest tests so they run in parallel
# under `ctest --parallel` instead of one serial runner invocation. EVMONE_SPEC_TESTS is a
# list of base directories; below each, the fixture directories three levels under
# `state_tests` / `blockchain_tests` become one test each, running `evmone test` over the
# directory. A tree shallower than that is registered at its leaves, so every fixture runs
# exactly once: no fixture directory holds both files and subdirectories.
#
# Grouping by directory rather than by file is what makes this a win: ctest's per-test cost
# is paid on one thread whatever --parallel says, so registering every file made the run
# slower than the serial one it replaces.
#
# Tests are named by the directory path relative to the base, so fixtures from different
# sources stay distinct. The fixtures must exist at configure time. Select a type with
# `ctest -L state_tests`, a source with `ctest -R <base-subdir>`.
set(EVMONE_SPEC_TESTS "" CACHE STRING
    "List of base directories scanned for execution-spec-test fixtures to register as ctest tests")

function(evmone_register_spec_test_dir base dir type depth)
    file(GLOB entries LIST_DIRECTORIES true "${dir}/*")
    set(subdirs "")
    foreach(entry ${entries})
        if(IS_DIRECTORY "${entry}")
            list(APPEND subdirs "${entry}")
        endif()
    endforeach()

    # Three levels down, or a leaf above it, is one test. Otherwise recurse.
    if(depth LESS 3 AND subdirs)
        foreach(subdir ${subdirs})
            math(EXPR next "${depth} + 1")
            evmone_register_spec_test_dir("${base}" "${subdir}" "${type}" ${next})
        endforeach()
        return()
    endif()

    file(RELATIVE_PATH name "${base}" "${dir}")
    add_test(NAME "${name}" COMMAND evmone-cli test "${dir}")
    # Per-process profraw so parallel coverage runs do not clobber each other.
    set_tests_properties("${name}" PROPERTIES LABELS "${type}"
        ENVIRONMENT "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/${type}-%m.profraw")
endfunction()

foreach(base ${EVMONE_SPEC_TESTS})
    foreach(type state_tests blockchain_tests)
        if(IS_DIRECTORY "${base}/fixtures/${type}")
            evmone_register_spec_test_dir("${base}" "${base}/fixtures/${type}" "${type}" 0)
        endif()
    endforeach()
endforeach()
