# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

set(PREFIX ${PROJECT_NAME}/integration)

if(TARGET evmone-cli)

    add_test(NAME ${PREFIX}/run COMMAND evmone-cli run 6000)
    set_tests_properties(
        ${PREFIX}/run PROPERTIES PASS_REGULAR_EXPRESSION
        "Result:   success\nGas used: 3\n")

    add_test(NAME ${PREFIX}/run_rev COMMAND evmone-cli run --rev TangerineWhistle 00)
    set_tests_properties(
        ${PREFIX}/run_rev PROPERTIES PASS_REGULAR_EXPRESSION
        "Executing on TangerineWhistle")

    add_test(NAME ${PREFIX}/run_bad_rev COMMAND evmone-cli run --rev NoSuchRev 00)
    set_tests_properties(
        ${PREFIX}/run_bad_rev PROPERTIES PASS_REGULAR_EXPRESSION
        "unknown revision: NoSuchRev")

    add_test(NAME ${PREFIX}/version COMMAND evmone-cli --version)
    set_tests_properties(
        ${PREFIX}/version PROPERTIES PASS_REGULAR_EXPRESSION "evmone")

    # Test loading code from a file via @file syntax.
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/code.hex "60 00 80 80 80 80 01 01 01 02 00")
    add_test(NAME ${PREFIX}/file_input COMMAND evmone-cli run @${CMAKE_CURRENT_BINARY_DIR}/code.hex)
    set_tests_properties(
        ${PREFIX}/file_input PROPERTIES PASS_REGULAR_EXPRESSION
        "Result:   success\n")

    add_test(NAME ${PREFIX}/histogram COMMAND evmone-cli --histogram run 6000808080800101010200)
    set_tests_properties(
        ${PREFIX}/histogram PROPERTIES PASS_REGULAR_EXPRESSION
        "--- # HISTOGRAM depth=0
opcode,count
STOP,1
ADD,3
MUL,1
PUSH1,1
DUP1,4
")

    add_test(NAME ${PREFIX}/trace COMMAND evmone-cli --trace run 60006002800103)
    set_tests_properties(
        ${PREFIX}/trace PROPERTIES PASS_REGULAR_EXPRESSION
        "\"pc\":0,\"op\":96,\"gas\":\"0xf4240\",\"gasCost\":\"0x3\",\"memSize\":0,\"stack\":\\[\\],\"depth\":1,\"refund\":0,\"opName\":\"PUSH1\"}
{\"pc\":2,\"op\":96,\"gas\":\"0xf423d\",\"gasCost\":\"0x3\",\"memSize\":0,\"stack\":\\[\"0x0\"\\],\"depth\":1,\"refund\":0,\"opName\":\"PUSH1\"}
{\"pc\":4,\"op\":128,\"gas\":\"0xf423a\",\"gasCost\":\"0x3\",\"memSize\":0,\"stack\":\\[\"0x0\",\"0x2\"\\],\"depth\":1,\"refund\":0,\"opName\":\"DUP1\"}
{\"pc\":5,\"op\":1,\"gas\":\"0xf4237\",\"gasCost\":\"0x3\",\"memSize\":0,\"stack\":\\[\"0x0\",\"0x2\",\"0x2\"\\],\"depth\":1,\"refund\":0,\"opName\":\"ADD\"}
{\"pc\":6,\"op\":3,\"gas\":\"0xf4234\",\"gasCost\":\"0x3\",\"memSize\":0,\"stack\":\\[\"0x0\",\"0x4\"\\],\"depth\":1,\"refund\":0,\"opName\":\"SUB\"}
")

endif()

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

add_subdirectory(blockchaintest)
add_subdirectory(export)
add_subdirectory(statetest)
add_subdirectory(t8n)

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)

