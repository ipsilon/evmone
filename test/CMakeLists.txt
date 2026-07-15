# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2019-2020 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

set(evmone_private_include_dir ${PROJECT_SOURCE_DIR}/lib)

hunter_add_package(CLI11)
find_package(CLI11 CONFIG REQUIRED)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

hunter_add_package(benchmark)
find_package(benchmark CONFIG REQUIRED)

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
        # Classify by the top-level (shallowest) test-type directory. A blockchain
        # test may contain a nested state_tests/ subdirectory (imported static
        # tests), so match the first type directory in the path rather than any
        # occurrence, which would misclassify such a test as a state test. The
        # leading and trailing slashes keep blockchain_tests_engine, _sync and
        # transaction_tests (which are not run here) out.
        string(FIND "/${name}" "/state_tests/" state_pos)
        string(FIND "/${name}" "/blockchain_tests/" bc_pos)
        if(NOT bc_pos EQUAL -1 AND (state_pos EQUAL -1 OR bc_pos LESS state_pos))
            set(runner evmone-blockchaintest)
            set(type blockchain_tests)
        elseif(NOT state_pos EQUAL -1)
            set(runner evmone-statetest)
            set(type state_tests)
        else()
            continue()
        endif()
        add_test(NAME "${name}" COMMAND ${runner} "${file}")
        # Per-process profraw so parallel coverage runs do not clobber each other.
        set_tests_properties("${name}" PROPERTIES LABELS "${type}"
            ENVIRONMENT "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/${type}-%m.profraw")
    endforeach()
endforeach()
