# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for running test fixture files.

set(PREFIX ${PREFIX}/test)

# A file holding a case which is not a fixture at all, beside one which runs.
add_test(NAME ${PREFIX}/fixture_fault COMMAND evmone-statetest
    ${CMAKE_CURRENT_SOURCE_DIR}/testcmd_fault)
set_tests_properties(${PREFIX}/fixture_fault PROPERTIES WILL_FAIL TRUE)

# WILL_FAIL above accepts any non-zero exit, and a fault downgraded to a skip exits non-zero too,
# so the counts are what distinguish the two.
add_test(NAME ${PREFIX}/fixture_fault_is_not_a_skip COMMAND evmone-statetest
    ${CMAKE_CURRENT_SOURCE_DIR}/testcmd_fault)
set_tests_properties(
    ${PREFIX}/fixture_fault_is_not_a_skip PROPERTIES PASS_REGULAR_EXPRESSION "1 failed, 0 passed")

# Selecting only the case which is not a fixture must still fault.
add_test(NAME ${PREFIX}/fixture_fault_survives_filter COMMAND evmone-statetest
    ${CMAKE_CURRENT_SOURCE_DIR}/testcmd_fault -k b_not_a_fixture)
set_tests_properties(
    ${PREFIX}/fixture_fault_survives_filter PROPERTIES
    PASS_REGULAR_EXPRESSION "1 failed, 0 passed")

# A case whose load throws takes the rest of the file with it: the whole file is loaded before
# any of it runs, so the case after it is never reached and the failure is named after the file
# rather than the case it came from. FAILED pins that as a failure: a PASS_REGULAR_EXPRESSION
# makes CTest ignore the exit code, and a fault downgraded to a skip names the file too.
add_test(NAME ${PREFIX}/case_after_exception COMMAND evmone-statetest
    ${CMAKE_CURRENT_SOURCE_DIR}/testcmd_cases)
set_tests_properties(
    ${PREFIX}/case_after_exception PROPERTIES
    PASS_REGULAR_EXPRESSION
    "collected 1 test.*FAILED[^\n]*case_after_exception\\.json[^\n]*exception"
    FAIL_REGULAR_EXPRESSION "b_wrong_state_root"
)


get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)

add_subdirectory(blockchaintest)
add_subdirectory(statetest)
