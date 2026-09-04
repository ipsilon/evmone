# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for the `evmone run` subcommand.

set(PREFIX ${PREFIX}/run)

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

# Code read from a file rather than the command line, which the @ prefix asks for. The file is
# checked in beside the test, so it is read and edited like every other fixture here.
add_test(NAME ${PREFIX}/file_input COMMAND evmone-cli run @${CMAKE_CURRENT_SOURCE_DIR}/code.hex)
set_tests_properties(
    ${PREFIX}/file_input PROPERTIES PASS_REGULAR_EXPRESSION
    # An unread file runs as empty code, which succeeds too; the gas is the file's code running.
    "Result:   success\nGas used: 29\n")

# What each tracer writes is pinned by the tracing unit tests, which drive them directly. These
# two are the app-level flags reaching the VM, which only a run of the command shows.
add_test(NAME ${PREFIX}/histogram COMMAND evmone-cli --histogram run 6000808080800101010200)
set_tests_properties(
    ${PREFIX}/histogram PROPERTIES PASS_REGULAR_EXPRESSION "--- # HISTOGRAM depth=0")

add_test(NAME ${PREFIX}/trace COMMAND evmone-cli --trace run 60006002800103)
set_tests_properties(
    ${PREFIX}/trace PROPERTIES PASS_REGULAR_EXPRESSION "\"opName\":\"PUSH1\"")

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
