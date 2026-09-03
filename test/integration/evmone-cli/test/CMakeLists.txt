# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for running test fixture files.

set(PREFIX ${PREFIX}/test)

# FIXME: move `evmone-cli test` tests to subdir: integration/test (sibling to integration/t8n).
#        we can add one more dir: integration/evmone-cli/{t8n,test,run}.
set(DATA ${CMAKE_CURRENT_SOURCE_DIR})

# One command runs both fixture formats, deciding the fixture format on the go:
# file 2 has _info.fixture-format, others are recognized by field names.
add_test(NAME ${PREFIX}/test_mixed_formats COMMAND evmone-cli test
    ${DATA}/statetest/tests1/SuiteA/test1.json ${DATA}/blockchaintest/eip7778_block_gas.json
    ${DATA}/blockchaintest/unrecovered_sender_blob_gas.json)
set_tests_properties(
    ${PREFIX}/test_mixed_formats PROPERTIES PASS_REGULAR_EXPRESSION "= 3 passed in")

# A file recognized as "not a test" is skipped.
add_test(NAME ${PREFIX}/test_not_a_test COMMAND evmone-cli test
    ${DATA}/statetest/tests1/SuiteA/index.json)
set_tests_properties(
    ${PREFIX}/test_not_a_test PROPERTIES PASS_REGULAR_EXPRESSION
    "SKIPPED[^\n]*index\\.json - not a test.*0 passed, 1 skipped in")

# Check exit code for: passed, failed, skipped (PASS_REGULAR_EXPRESSION ignores exit code).
add_test(NAME ${PREFIX}/test_exit_ok COMMAND evmone-cli test ${DATA}/testcmd)
add_test(NAME ${PREFIX}/test_exit_fail COMMAND evmone-cli test ${DATA}/testcmd_fault)
set_tests_properties(${PREFIX}/test_exit_fail PROPERTIES WILL_FAIL TRUE)
add_test(NAME ${PREFIX}/test_exit_skipped COMMAND evmone-cli test ${DATA}/testcmd_skipped)
set_tests_properties(${PREFIX}/test_exit_skipped PROPERTIES WILL_FAIL TRUE)

# A declined fixture should not kill the whole file.
add_test(NAME ${PREFIX}/test_dir_declined COMMAND evmone-cli test ${DATA}/testcmd)
set_tests_properties(
    ${PREFIX}/test_dir_declined PROPERTIES PASS_REGULAR_EXPRESSION
    "SKIPPED[^\n]*::a_bad_rlp - tests with invalidly rlp-encoded blocks[^\n]*\nSKIPPED[^\n]*::c_engine - unsupported fixture format.*= 1 passed in")

# The -k filter should not filter out a "not a test".
add_test(NAME ${PREFIX}/test_dir_fault COMMAND evmone-cli test
    ${DATA}/testcmd_fault -k b_not_a_test)
set_tests_properties(
    ${PREFIX}/test_dir_fault PROPERTIES PASS_REGULAR_EXPRESSION "1 failed, 0 passed")

# A failed fixture should not kill the whole file.
add_test(NAME ${PREFIX}/test_after_exception COMMAND evmone-cli test ${DATA}/testcmd_fault)
set_tests_properties(
    ${PREFIX}/test_after_exception PROPERTIES PASS_REGULAR_EXPRESSION
    "collected 1 file\n.*c_load_error:\n    exception.*d_bad_root:.*state root.*1 failed, 0 passed in")

# The -k filter should filter out the failing fixture.
add_test(NAME ${PREFIX}/test_dir_filter COMMAND evmone-cli test
    ${DATA}/statetest/filter -k passing_test_case --trace-summary)
set_tests_properties(
    ${PREFIX}/test_dir_filter PROPERTIES
    PASS_REGULAR_EXPRESSION "\"pass\":true"
    FAIL_REGULAR_EXPRESSION "failing_test_case")

# A file is one test, so a file with nothing to run counts once as skipped however many
# fixtures it declined: all_unsupported declines twice and both are named.
add_test(NAME ${PREFIX}/test_dir_skipped COMMAND evmone-cli test ${DATA}/testcmd_skipped)
set_tests_properties(
    ${PREFIX}/test_dir_skipped PROPERTIES PASS_REGULAR_EXPRESSION
    "all_unsupported\\.json::a_engine - unsupported fixture format: \"blockchain_test_engine\"\nSKIPPED[^\n]*::b_engine_x - .*empty\\.json - not a test.*pre_alloc\\.json - not a test.*0 passed, 5 skipped in")

# The --histogram enables also test name printing.
# FIXME: This should print the fixture name, not the test file. This can be fixed later when we will add --verbosity flag. If so, remove file printing now.
add_test(NAME ${PREFIX}/test_histogram COMMAND evmone-cli test --histogram
    ${DATA}/statetest/tests1/SuiteA/test1.json)
set_tests_properties(
    ${PREFIX}/test_histogram PROPERTIES PASS_REGULAR_EXPRESSION
    "test1\\.json
--- # HISTOGRAM.*= 1 passed in")

# The --trace enables also test name printing.
add_test(NAME ${PREFIX}/test_trace COMMAND evmone-cli test --trace
    ${DATA}/statetest/tests1/SuiteA/test1.json)
set_tests_properties(
    ${PREFIX}/test_trace PROPERTIES PASS_REGULAR_EXPRESSION
    "test1\\.json\n\\{\"pc\":0,[^\n]*\"opName\":\"PUSH1\"\\}.*= 1 passed in")

# A named file is loaded whatever its extension.
add_test(NAME ${PREFIX}/test_file_unparsed COMMAND evmone-cli test
    ${DATA}/statetest/tests1/SuiteA/notes.txt)
set_tests_properties(
    ${PREFIX}/test_file_unparsed PROPERTIES PASS_REGULAR_EXPRESSION
    "FAILED[^\n]*notes\\.txt - exception[^\n]*parse error.*1 failed, 0 passed in")


get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)

add_subdirectory(blockchaintest)
add_subdirectory(statetest)
