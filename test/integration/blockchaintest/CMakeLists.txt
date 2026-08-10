# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2022 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for evmone-blockchaintest.

set(PREFIX ${PREFIX}/blockchaintest)
set(TESTS1 ${CMAKE_CURRENT_SOURCE_DIR})

add_test(
    NAME ${PREFIX}/json_test
    COMMAND evmone-blockchaintest ${TESTS1}/test.json
)
set_tests_properties(
    ${PREFIX}/json_test PROPERTIES
    # Make sure both tests in the file are executed (both should fail).
    PASS_REGULAR_EXPRESSION ".*2 tests from"
)

# Exercise block-level gas accounting (EIP-7778).
add_test(
    NAME ${PREFIX}/eip7778_block_gas
    COMMAND evmone-blockchaintest ${TESTS1}/eip7778_block_gas.json
)

# A block invalid by its signature must not be judged by another check.
add_test(
    NAME ${PREFIX}/unrecovered_sender_blob_gas
    COMMAND evmone-blockchaintest ${TESTS1}/unrecovered_sender_blob_gas.json
)
add_test(
    NAME ${PREFIX}/unrecovered_sender_deposit_log
    COMMAND evmone-blockchaintest ${TESTS1}/unrecovered_sender_deposit_log.json
)

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
