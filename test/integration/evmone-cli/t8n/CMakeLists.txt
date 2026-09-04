# evmone: Fast Ethereum Virtual Machine implementation
# Copyright 2024 The evmone Authors.
# SPDX-License-Identifier: Apache-2.0

# Integration tests for the `evmone t8n` subcommand.

set(PREFIX ${PREFIX}/t8n)

# Nothing removes what a case writes, so a run which wrote nothing would be checked against the
# output of the last one. Every case waits for this.
add_test(NAME ${PREFIX}/clean_output_dirs COMMAND ${CMAKE_COMMAND} -E rm -rf
    ${CMAKE_CURRENT_BINARY_DIR}/london_base_fee
    ${CMAKE_CURRENT_BINARY_DIR}/berlin_base_fee
    ${CMAKE_CURRENT_BINARY_DIR}/cancun_create_tx
    ${CMAKE_CURRENT_BINARY_DIR}/cancun_create_tx_trace
    ${CMAKE_CURRENT_BINARY_DIR}/prague_empty_requests
    ${CMAKE_CURRENT_BINARY_DIR}/prague_pre_state_only
    ${CMAKE_CURRENT_BINARY_DIR}/osaka_blob_schedule
    ${CMAKE_CURRENT_BINARY_DIR}/chainid_hex_max)
set_tests_properties(${PREFIX}/clean_output_dirs PROPERTIES FIXTURES_SETUP outputs)

set(TEST_CASE london_base_fee)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/base_fee
    COMMAND
    evmone-cli t8n
    --state.fork London
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    # What the fee comes to is a unit test; this is --output.result reaching the file named.
    PASS_REGULAR_EXPRESSION [=["currentBaseFee"]=]
)


set(TEST_CASE berlin_base_fee)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/base_fee
    COMMAND
    evmone-cli t8n
    --state.fork Berlin
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION [=["gasUsed"]=]
    # Only a run shows that --state.fork Berlin reaches t8n as a revision before London.
    FAIL_REGULAR_EXPRESSION currentBaseFee
)


set(TEST_CASE cancun_create_tx)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_CASE}
    COMMAND
    evmone-cli t8n
    --state.fork Cancun
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
    --output.alloc outAlloc.json
    --opcode.count opcodes.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION [=["gasUsed"]=]
)

add_test(
    NAME ${PREFIX}/${TEST_CASE}/outAlloc.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/outAlloc.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/outAlloc.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    # What t8n computed is a unit test; this is --output.alloc reaching the file named.
    PASS_REGULAR_EXPRESSION [=["0x000f3df6d732807ef1319fb7b8bb8522d0beac02"]=]
)

add_test(
    NAME ${PREFIX}/${TEST_CASE}/opcodes.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/opcodes.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/opcodes.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION [=[\{
  "STOP": 1,
  "ADD": 1,
  "MOD": 1,
  "EQ": 1,
  "CALLER": 1,
  "CALLDATALOAD": 1,
  "TIMESTAMP": 2,
  "SSTORE": 2,
  "JUMPI": 1,
  "JUMPDEST": 1,
  "PUSH0": 2,
  "PUSH1": 2,
  "PUSH3": 2,
  "PUSH20": 1,
  "DUP2": 1,
  "SWAP1": 1,
  "RETURN": 1
\}]=]
)

set(TEST_CASE prague_empty_requests)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_CASE}
    COMMAND
    evmone-cli t8n
    --state.fork Prague
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION [=["requestsHash"]=]
    # The alloc holds both request contracts, so the hash is of no requests rather than of a
    # collection which failed.
    FAIL_REGULAR_EXPRESSION blockException
)

set(TEST_CASE osaka_blob_schedule)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_CASE}
    COMMAND
    evmone-cli t8n
    --state.fork Osaka
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --input.blobParams blobParams.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
string(
    JOIN ".*" EXPECTED_OUT
    # resolved blob parameters were read for Osaka
    [=["cumulativeGasUsed": ]=]
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION ${EXPECTED_OUT}
    # The alloc holds both request contracts, so the gas is a valid block's rather than a
    # rejected one's.
    FAIL_REGULAR_EXPRESSION blockException
)

set(TEST_CASE cancun_create_tx_trace)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cancun_create_tx
    COMMAND
    evmone-cli t8n
    --state.fork Cancun
    --state.reward 0
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
    --trace
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

# The first tx in this fixture is a successful CREATE with bytecode 0x60015ff3
# (PUSH1 0x01 PUSH0 RETURN). Its trace must be written to a per-tx file whose
# name includes the computed tx hash.
add_test(
    NAME ${PREFIX}/${TEST_CASE}/trace-0.jsonl
    COMMAND ${CMAKE_COMMAND} -E cat
    ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/trace-0-0x03141f8608bc78c8b8fb23275febeb6bf40bd348bd68b703a92bf2ce8ac8bdc0.jsonl
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/trace-0.jsonl PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    PASS_REGULAR_EXPRESSION [=["opName":"PUSH1".*"opName":"PUSH0".*"opName":"RETURN"]=]
)


add_test(NAME ${PREFIX}/bad_fork COMMAND evmone-cli t8n --state.fork NoSuchRev)
set_tests_properties(
    ${PREFIX}/bad_fork PROPERTIES PASS_REGULAR_EXPRESSION "unknown revision")


# Check if chainid max value is accepted.
add_test(
    NAME ${PREFIX}/chainid_hex_max
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/base_fee
    COMMAND
    evmone-cli t8n
    --state.fork Paris
    --state.chainid 0xffffffffffffffff
    --input.env env.json
)


# Exercises both --output.body (RLP-encoded transactions) and
# --state.reward -1 (pre-state-only mode that emits just the state root).
set(TEST_CASE prague_pre_state_only)

add_test(
    NAME ${PREFIX}/${TEST_CASE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_CASE}
    COMMAND
    evmone-cli t8n
    --state.fork Prague
    --state.reward -1
    --state.chainid 1
    --input.alloc alloc.json
    --input.txs txs.json
    --input.env env.json
    --output.basedir ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}
    --output.result out.json
    --output.body body.rlp
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE} PROPERTIES FIXTURES_REQUIRED "${TEST_CASE};outputs")

add_test(
    NAME ${PREFIX}/${TEST_CASE}/out.json
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/out.json
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/out.json PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    # The pre-state holds no request contract, so only the skipped system calls keep the block
    # valid: this is what tells the mode apart from a reward of zero.
    PASS_REGULAR_EXPRESSION [=["stateRoot"]=]
    FAIL_REGULAR_EXPRESSION blockException
)

add_test(
    NAME ${PREFIX}/${TEST_CASE}/body.rlp
    COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/${TEST_CASE}/body.rlp
)
set_tests_properties(
    ${PREFIX}/${TEST_CASE}/body.rlp PROPERTIES
    FIXTURES_CLEANUP ${TEST_CASE}
    # The whole encoding: an empty transaction list is hex too, and is what a run which never read
    # --input.txs writes.
    PASS_REGULAR_EXPRESSION
    "^0xf84ef84c8032830186a08080801ba0468a915f087692bb9be503831a3dfef2cf9c8dee26deb40ff2ec99e8d22665aea05cedae0810c3851ecd1004bfdbfe6ddc7753c2d665993bb01ce75af7857b13dc"
)


get_directory_property(ALL_TESTS TESTS)
set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/integration-%p.profraw)
