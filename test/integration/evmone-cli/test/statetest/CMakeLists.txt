# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for evmone-statetest.

set(PREFIX ${PREFIX}/statetest)
set(TESTS1 ${CMAKE_CURRENT_SOURCE_DIR}/tests1)
set(TESTS2 ${CMAKE_CURRENT_SOURCE_DIR}/tests2)
set(TESTS_FILTER ${CMAKE_CURRENT_SOURCE_DIR}/filter)

add_test(
    NAME ${PREFIX}/no_arguments
    COMMAND evmone-statetest
)
set_tests_properties(
    ${PREFIX}/no_arguments PROPERTIES
    PASS_REGULAR_EXPRESSION "path is required"
)

# A test collected from a directory is listed as its file. SuiteA also holds an index.json and a
# notes.txt, neither of which is a test; both sort before test1.json, so collecting either would
# show up in the listing.
add_test(
    NAME ${PREFIX}/tests1_list
    COMMAND evmone-statetest ${TESTS1} --collect-only
)
set_tests_properties(
    ${PREFIX}/tests1_list PROPERTIES
    PASS_REGULAR_EXPRESSION "tests1[^\n]*T\\.json\n[^\n]*tests1[^\n]*test1\\.json\n[^\n]*tests1[^\n]*test2_multi\\.json"
)

# Given the file directly, each test case in it is listed on its own.
add_test(
    NAME ${PREFIX}/single_file_list
    COMMAND evmone-statetest ${TESTS1}/SuiteA/test2_multi.json --collect-only
)
set_tests_properties(
    ${PREFIX}/single_file_list PROPERTIES
    PASS_REGULAR_EXPRESSION "test2_multi\\.json::test_case_1\n[^\n]*test2_multi\\.json::test_case_2"
)

# Several roots are collected in the order given, not regrouped by suite as gtest listed them.
# T.json holds no test cases, so naming it directly contributes no line.
add_test(
    NAME ${PREFIX}/multiple_args_list
    COMMAND evmone-statetest ${TESTS1} ${TESTS2} ${TESTS1}/B/T.json ${TESTS1}/SuiteA --collect-only
)
set_tests_properties(
    ${PREFIX}/multiple_args_list PROPERTIES
    PASS_REGULAR_EXPRESSION "tests1[^\n]*T\\.json\n[^\n]*tests1[^\n]*test1\\.json\n[^\n]*tests1[^\n]*test2_multi\\.json\n[^\n]*tests2[^\n]*test1\\.json\n[^\n]*tests1[^\n]*test1\\.json\n[^\n]*tests1[^\n]*test2_multi\\.json"
)

add_test(
    NAME ${PREFIX}/invalid_path
    COMMAND evmone-statetest invalid.json
)
set_tests_properties(
    ${PREFIX}/invalid_path PROPERTIES
    PASS_REGULAR_EXPRESSION "path: Path does not exist: invalid\\.json"
)

add_test(
    NAME ${PREFIX}/multi_test
    COMMAND evmone-statetest ${TESTS1}/SuiteA/test2_multi.json
)
set_tests_properties(
    ${PREFIX}/multi_test PROPERTIES
    # Make sure both tests in the file are executed (both should fail).
    PASS_REGULAR_EXPRESSION "test_case_1.*test_case_2"
)

add_test(
    NAME ${PREFIX}/trace
    COMMAND evmone-statetest ${TESTS1}/SuiteA/test1.json --trace
)
set_tests_properties(
    ${PREFIX}/trace PROPERTIES
    # The instruction trace is pinned by the tracing unit tests, which drive the tracer directly.
    # What --trace adds to it is the runner's summary of the test, so that line is what is checked.
    PASS_REGULAR_EXPRESSION
    "\"pass\":true,\"gasUsed\":\"0xa868\",\"stateRoot\":\"0xe8010ce590f401c9d61fef8ab05bea9bcec24281b795e5868809bc4e515aa530\""
)

add_test(
    NAME ${PREFIX}/filter
    COMMAND evmone-statetest ${TESTS_FILTER}/one_failing_of_two.json -k passing_test_case
)
set_tests_properties(
    ${PREFIX}/filter PROPERTIES
    # The whole summary is what proves a case ran and passed: a count alone would hold just as
    # well if the filter selected nothing, and ctest stops checking the exit code once a
    # PASS_REGULAR_EXPRESSION is set. The summary names failures first, so this matches only a
    # run with none.
    PASS_REGULAR_EXPRESSION "= 1 passed in"
    FAIL_REGULAR_EXPRESSION "failing_test_case"
)

# Over a directory the filter is applied per case inside the file's test, not at registration as
# it is above. The summary line is what proves a case ran: forbidding the other name alone would
# hold just as well if the filter dropped every case.
add_test(
    NAME ${PREFIX}/filter_directory
    COMMAND evmone-statetest ${TESTS_FILTER} -k passing_test_case --trace-summary
)
set_tests_properties(
    ${PREFIX}/filter_directory PROPERTIES
    PASS_REGULAR_EXPRESSION "\"pass\":true"
    FAIL_REGULAR_EXPRESSION "failing_test_case"
)

# --ignore drops a path from the collection. The flags come first, as they do in CI: an --ignore
# that swallowed a positional path would leave the roots after it uncollected.
add_test(
    NAME ${PREFIX}/ignore
    COMMAND evmone-statetest --ignore B ${TESTS1} ${TESTS2} --collect-only
)
set_tests_properties(
    ${PREFIX}/ignore PROPERTIES
    PASS_REGULAR_EXPRESSION "tests1[^\n]*test1\\.json\n[^\n]*tests1[^\n]*test2_multi\\.json\n[^\n]*tests2[^\n]*test1\\.json"
    FAIL_REGULAR_EXPRESSION "T\\.json"
)

# Selecting nothing fails rather than passing vacuously. WILL_FAIL only asserts a nonzero exit;
# the driver unit tests pin the code itself.
add_test(
    NAME ${PREFIX}/nothing_collected
    COMMAND evmone-statetest ${TESTS1} --ignore B --ignore SuiteA
)
set_tests_properties(${PREFIX}/nothing_collected PROPERTIES WILL_FAIL TRUE)

# A file which cannot be loaded is a failure of the listing too, not a name in it.
add_test(
    NAME ${PREFIX}/collect_only_unloadable
    COMMAND evmone-statetest ${CMAKE_CURRENT_SOURCE_DIR}/tests1/SuiteA/notes.txt --collect-only
)
set_tests_properties(${PREFIX}/collect_only_unloadable PROPERTIES WILL_FAIL TRUE)

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
