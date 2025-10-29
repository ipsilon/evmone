# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2023 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Commands and tests for exporting evmone's tests to JSON formats

set(PREFIX ${PREFIX}/export)
set(EXPORT_DIR fixtures)

add_test(
    # Clean the export directory before exporting to delete potentially outdated tests.
    NAME ${PREFIX}/clean_export_dir
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${EXPORT_DIR}
)
set_tests_properties(
    ${PREFIX}/clean_export_dir PROPERTIES
    FIXTURES_SETUP EXPORT_PREPARATION
)

add_test(
    NAME ${PREFIX}/export_state_tests
    COMMAND evmone-unittests --gtest_filter=state_transition.*
)
set_tests_properties(
    ${PREFIX}/export_state_tests PROPERTIES
    ENVIRONMENT "EVMONE_EXPORT_TESTS=${EXPORT_DIR}/state_tests;LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw"
    FIXTURES_SETUP EXPORT_STATE_TESTS
    FIXTURES_REQUIRED EXPORT_PREPARATION
)

add_test(
    NAME ${PREFIX}/execute_exported_state_tests
    # TODO: Broken exported tests are filtered out.
    COMMAND evmone-statetest ${EXPORT_DIR}/state_tests --gtest_filter=-*block.*
)
set_tests_properties(
    ${PREFIX}/execute_exported_state_tests PROPERTIES
    FIXTURES_REQUIRED EXPORT_STATE_TESTS
)
