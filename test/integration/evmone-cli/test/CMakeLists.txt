# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for running test fixture files.

set(PREFIX ${PREFIX}/test)

set(DATA ${CMAKE_CURRENT_SOURCE_DIR})

# One command runs both fixture formats, deciding the fixture format on the go:
# file 2 has _info.fixture-type, others are recognized by field names.
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

# Over a directory a test is a whole file, so a case the loader refuses must not take the
# rest of the file with it: the file passes on the case beside it, the refused one first.
# The file's own verdict says nothing about what it declined, so each declined case is named.
add_test(NAME ${PREFIX}/test_dir_declined COMMAND evmone-cli test ${DATA}/testcmd)
set_tests_properties(
    ${PREFIX}/test_dir_declined PROPERTIES PASS_REGULAR_EXPRESSION
    "a_bad_rlp: tests with invalidly rlp-encoded blocks.*c_engine: unsupported fixture format.*= 1 passed in")

# A PASS_REGULAR_EXPRESSION makes CTest ignore the exit code, so these two assert on nothing
# else: a sound fixture directory succeeds, and a fault in one fails.
add_test(NAME ${PREFIX}/test_exit_ok COMMAND evmone-cli test ${DATA}/testcmd)

add_test(NAME ${PREFIX}/test_exit_fail COMMAND evmone-cli test ${DATA}/testcmd_fault)
set_tests_properties(${PREFIX}/test_exit_fail PROPERTIES WILL_FAIL TRUE)

# WILL_FAIL above accepts any non-zero exit, and a fault downgraded to a skip exits non-zero
# too, so the counts are what distinguish the two. Whether a file is a fixture file is a
# property of the file, not of what -k selected: the case which is not a test still faults.
add_test(NAME ${PREFIX}/test_dir_fault COMMAND evmone-cli test
    ${DATA}/testcmd_fault -k b_not_a_test)
set_tests_properties(
    ${PREFIX}/test_dir_fault PROPERTIES PASS_REGULAR_EXPRESSION "1 failed, 0 passed")

# Over a directory -k selects within the file's test, so the summary is what proves a case
# ran: a count alone would hold just as well if the filter had dropped every case. The
# summary appears only with --trace-summary, which nothing else here asks for.
add_test(NAME ${PREFIX}/test_dir_filter COMMAND evmone-cli test
    ${DATA}/statetest/filter -k passing_test_case --trace-summary)
set_tests_properties(
    ${PREFIX}/test_dir_filter PROPERTIES
    PASS_REGULAR_EXPRESSION "\"pass\":true"
    FAIL_REGULAR_EXPRESSION "failing_test_case")

# A case whose load throws must not abandon the cases after it, so both faults are reported.
add_test(NAME ${PREFIX}/test_after_exception COMMAND evmone-cli test ${DATA}/testcmd_cases)
set_tests_properties(
    ${PREFIX}/test_after_exception PROPERTIES
    # "collected 1 test" keeps this on the directory form, where the whole file is one test
    # and the per-case catch is what keeps the second fault reachable.
    PASS_REGULAR_EXPRESSION
    "collected 1 test.*a_load_error:\n    exception.*b_bad_root:.*state root")

# Collected from a directory a file is one test, so a file with nothing to run is skipped
# whole, named by the first reason which explains it: all_unsupported declines twice, and
# the format named is the first one's.
add_test(NAME ${PREFIX}/test_dir_skipped COMMAND evmone-cli test ${DATA}/testcmd_skipped)
set_tests_properties(
    ${PREFIX}/test_dir_skipped PROPERTIES
    PASS_REGULAR_EXPRESSION
    "all_unsupported\\.json - unsupported fixture format: \"blockchain_test_engine\"\nSKIPPED[^\n]*empty\\.json - not a test.*pre_alloc\\.json - not a test.*0 passed, 5 skipped in")

# Nothing to run is not nothing to report: the run fails rather than passing empty.
add_test(NAME ${PREFIX}/test_dir_skipped_exit COMMAND evmone-cli test ${DATA}/testcmd_skipped)
set_tests_properties(${PREFIX}/test_dir_skipped_exit PROPERTIES WILL_FAIL TRUE)

# Naming the file instead of the directory makes each case its own test, and a fault in one
# is still a fault: the case beside it runs.
add_test(NAME ${PREFIX}/test_file_fault COMMAND evmone-cli test
    ${DATA}/testcmd_fault/unrecognised_case.json)
set_tests_properties(
    ${PREFIX}/test_file_fault PROPERTIES PASS_REGULAR_EXPRESSION
    "FAILED[^\n]*unrecognised_case\\.json::b_not_a_test.*1 failed, 1 passed in")

# -k selects among the cases of a named file too.
add_test(NAME ${PREFIX}/test_file_filter COMMAND evmone-cli test
    ${DATA}/testcmd_fault/unrecognised_case.json -k a_runs)
set_tests_properties(
    ${PREFIX}/test_file_filter PROPERTIES PASS_REGULAR_EXPRESSION "= 1 passed in")

# Naming a file whose cases this tool does not run skips them one by one, where collecting
# the same file from a directory would skip it whole.
add_test(NAME ${PREFIX}/test_file_declined COMMAND evmone-cli test
    ${DATA}/testcmd/one_unsupported_case.json)
set_tests_properties(
    ${PREFIX}/test_file_declined PROPERTIES PASS_REGULAR_EXPRESSION
    "SKIPPED[^\n]*c_engine - unsupported fixture format.*= 1 passed, 2 skipped in")

# --trace and --histogram reach the runner through the app-level flags, and turn the progress
# row into a line naming each test so what each writes can be told apart.
add_test(NAME ${PREFIX}/test_histogram COMMAND evmone-cli test --histogram
    ${DATA}/statetest/tests1/SuiteA/test1.json)
set_tests_properties(
    ${PREFIX}/test_histogram PROPERTIES PASS_REGULAR_EXPRESSION
    "test1\\.json::test1
--- # HISTOGRAM.*= 1 passed in")

add_test(NAME ${PREFIX}/test_trace COMMAND evmone-cli test --trace
    ${DATA}/statetest/tests1/SuiteA/test1.json)
set_tests_properties(
    ${PREFIX}/test_trace PROPERTIES PASS_REGULAR_EXPRESSION
    "test1\\.json::test1\n\\{\"pc\":0,[^\n]*\"opName\":\"PUSH1\"\\}.*= 1 passed in")

# A named file which does not parse names itself, because no case in it can be.
add_test(NAME ${PREFIX}/test_file_unparsed COMMAND evmone-cli test
    ${DATA}/testcmd_broken/truncated.json)
set_tests_properties(
    ${PREFIX}/test_file_unparsed PROPERTIES PASS_REGULAR_EXPRESSION
    "FAILED[^\n]*truncated\\.json - exception[^\n]*parse error.*1 failed, 0 passed in")


get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)

add_subdirectory(blockchaintest)
add_subdirectory(statetest)
