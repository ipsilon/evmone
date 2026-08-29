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

# Exercise block-level gas accounting (EIP-7778), with tracing on so that flag is covered too.
add_test(
    NAME ${PREFIX}/eip7778_block_gas
    COMMAND evmone-blockchaintest ${TESTS1}/eip7778_block_gas.json --trace
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

# A block with expectException but no rlp_decoded is a shape the loader does not support. As a
# single file it is reported and nothing is registered.
add_test(
    NAME ${PREFIX}/unsupported_rlp
    COMMAND evmone-blockchaintest ${TESTS1}/unsupported_rlp.json
)
set_tests_properties(
    ${PREFIX}/unsupported_rlp PROPERTIES
    PASS_REGULAR_EXPRESSION "tests with invalidly rlp-encoded blocks are not supported"
)

# The directory form registers one test per file, which none of the tests above reach, and skips
# the unsupported one rather than failing on it.
add_test(
    NAME ${PREFIX}/directory
    COMMAND evmone-blockchaintest ${TESTS1}
)
set_tests_properties(
    ${PREFIX}/directory PROPERTIES
    PASS_REGULAR_EXPRESSION "SKIPPED \\] \\.unsupported_rlp"
)

# --collect-only lists what would run instead of running it, and --ignore drops a path from that
# list. Over a directory a test is listed as its file.
add_test(
    NAME ${PREFIX}/collect_only_directory
    COMMAND evmone-blockchaintest ${TESTS1} --collect-only --ignore unsupported_rlp.json
)
set_tests_properties(
    ${PREFIX}/collect_only_directory PROPERTIES
    PASS_REGULAR_EXPRESSION "eip7778_block_gas\\.json"
    FAIL_REGULAR_EXPRESSION "unsupported_rlp"
)

# Given the file directly, each test case in it is listed on its own.
add_test(
    NAME ${PREFIX}/collect_only_file
    COMMAND evmone-blockchaintest ${TESTS1}/test.json --collect-only
)
set_tests_properties(
    ${PREFIX}/collect_only_file PROPERTIES
    PASS_REGULAR_EXPRESSION "test\\.json::[^\n]*-call\\]\n[^\n]*test\\.json::[^\n]*-callcode\\]"
)

get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
