# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for evmone-statetest.

set(PREFIX ${PREFIX}/statetest)
set(TESTS1 ${CMAKE_CURRENT_SOURCE_DIR}/tests1)
set(TESTS2 ${CMAKE_CURRENT_SOURCE_DIR}/tests2)
set(TESTS_TX ${CMAKE_CURRENT_SOURCE_DIR}/tx)
set(TESTS_FILTER ${CMAKE_CURRENT_SOURCE_DIR}/filter)

add_test(
    NAME ${PREFIX}/no_arguments
    COMMAND evmone-statetest
)
set_tests_properties(
    ${PREFIX}/no_arguments PROPERTIES
    PASS_REGULAR_EXPRESSION "path is required"
)

add_test(
    NAME ${PREFIX}/tests1_list
    COMMAND evmone-statetest ${TESTS1} --gtest_list_tests
)
set_tests_properties(
    ${PREFIX}/tests1_list PROPERTIES
    PASS_REGULAR_EXPRESSION [[
B\.
  T
SuiteA\.
  test1
  test2_multi
]]
)

add_test(
    NAME ${PREFIX}/single_file_list
    COMMAND evmone-statetest ${TESTS1}/SuiteA/test1.json --gtest_list_tests
)
set_tests_properties(
    ${PREFIX}/single_file_list PROPERTIES
    PASS_REGULAR_EXPRESSION [[
.*test/integration/statetest/tests1/SuiteA/test1\.json\.
  test1
]]
)

add_test(
    NAME ${PREFIX}/multiple_args_list
    COMMAND evmone-statetest ${TESTS1} ${TESTS2} ${TESTS1}/B/T.json ${TESTS1}/SuiteA --gtest_list_tests
)
set_tests_properties(
    ${PREFIX}/multiple_args_list PROPERTIES
    PASS_REGULAR_EXPRESSION [[
B\.
  T
SuiteA\.
  test1
  test2_multi
  test1
\.
  test1
  test2_multi
]]
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
set(EXPECTED_TRACE [[
{"pc":0,"op":96,"gas":"0x5c878","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":2,"op":96,"gas":"0x5c875","gasCost":"0x3","memSize":0,"stack":["0x1"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":4,"op":1,"gas":"0x5c872","gasCost":"0x3","memSize":0,"stack":["0x1","0x1"],"depth":1,"refund":0,"opName":"ADD"}
{"pc":5,"op":96,"gas":"0x5c86f","gasCost":"0x3","memSize":0,"stack":["0x2"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":7,"op":85,"gas":"0x5c86c","gasCost":"0x0","memSize":0,"stack":["0x2","0x0"],"depth":1,"refund":0,"opName":"SSTORE"}
{"pc":8,"op":0,"gas":"0x57218","gasCost":"0x0","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"STOP"}
{"pass":true,"gasUsed":"0xa868","stateRoot":"0xe8010ce590f401c9d61fef8ab05bea9bcec24281b795e5868809bc4e515aa530"}
]])
# Escape regex special characters.
string(REPLACE "{" "\\{" EXPECTED_TRACE ${EXPECTED_TRACE})
string(REPLACE "}" "\\}" EXPECTED_TRACE ${EXPECTED_TRACE})
string(REPLACE "[" "\\[" EXPECTED_TRACE ${EXPECTED_TRACE})
string(REPLACE "]" "\\]" EXPECTED_TRACE ${EXPECTED_TRACE})
set_tests_properties(
    ${PREFIX}/trace PROPERTIES
    PASS_REGULAR_EXPRESSION ${EXPECTED_TRACE}
)

add_test(
    NAME ${PREFIX}/tx_invalid_nonce
    COMMAND evmone-statetest ${TESTS_TX}/invalid_nonce.json
)
set_tests_properties(
    ${PREFIX}/tx_invalid_nonce PROPERTIES
    PASS_REGULAR_EXPRESSION "unexpected invalid transaction: nonce too high"
)

add_test(
    NAME ${PREFIX}/filter
    COMMAND evmone-statetest ${TESTS_FILTER}/one_failing_of_two.json -k passing_test_case
)
set_tests_properties(
    ${PREFIX}/filter PROPERTIES
    FAIL_REGULAR_EXPRESSION "failing_test_case"
)

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
