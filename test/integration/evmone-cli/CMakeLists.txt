# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2021 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for the `evmone` command itself: what belongs to no subcommand.

set(PREFIX ${PREFIX}/evmone-cli)

# A subcommand's own name among another's arguments is an argument. CLI11 would otherwise
# take it as a second command and run that one instead, exiting 0 with the wrong work done:
# the dispatch tries run first, so a trailing "run" hijacked every other subcommand.
add_test(NAME ${PREFIX}/one_command_per_invocation COMMAND evmone-cli
    t8n --state.fork Cancun run 6000)
set_tests_properties(
    ${PREFIX}/one_command_per_invocation PROPERTIES
    PASS_REGULAR_EXPRESSION "arguments were not expected"
    FAIL_REGULAR_EXPRESSION "Result: *success")

add_test(NAME ${PREFIX}/one_command_per_invocation_reversed COMMAND evmone-cli
    run 6000 t8n)
set_tests_properties(
    ${PREFIX}/one_command_per_invocation_reversed PROPERTIES
    PASS_REGULAR_EXPRESSION "argument was not expected: t8n"
    FAIL_REGULAR_EXPRESSION "Result: *success")

# A PASS_REGULAR_EXPRESSION makes CTest ignore the exit code, so run them again for that alone.
add_test(NAME ${PREFIX}/one_command_per_invocation_exit_code COMMAND evmone-cli
    t8n --state.fork Cancun run 6000)
set_tests_properties(
    ${PREFIX}/one_command_per_invocation_exit_code PROPERTIES WILL_FAIL TRUE)

# t8n is given its required option here: without one the second command exits non-zero by itself,
# so the exit code would say nothing about whether it was taken as a command.
add_test(NAME ${PREFIX}/one_command_per_invocation_reversed_exit_code COMMAND evmone-cli
    run 6000 t8n --state.fork Cancun)
set_tests_properties(
    ${PREFIX}/one_command_per_invocation_reversed_exit_code PROPERTIES WILL_FAIL TRUE)

add_test(NAME ${PREFIX}/version COMMAND evmone-cli --version)
set_tests_properties(
    ${PREFIX}/version PROPERTIES PASS_REGULAR_EXPRESSION "evmone")


get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)

add_subdirectory(run)
add_subdirectory(t8n)
add_subdirectory(test)
