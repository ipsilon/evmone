// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// Tests of what the state test runner reports when a fixture does not hold. Those paths run only
/// when evmone disagrees with a fixture, which a green EEST run never does.

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <test/utils/statetest.hpp>

#include <sstream>

using namespace evmone;
using namespace evmone::test;

namespace
{
/// A transaction whose nonce is ahead of the sender's, in a fixture that expects it to succeed.
constexpr std::string_view NONCE_TOO_HIGH = R"({"invalid_nonce": {
    "env": {
        "currentBaseFee": "0x0a",
        "currentCoinbase": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "currentDifficulty": "0x020000",
        "currentGasLimit": "0xff112233445566",
        "currentNumber": "0x01",
        "currentRandom": "0x0000000000000000000000000000000000000000000000000000000000020000",
        "currentTimestamp": "0x03e8"
    },
    "post": {"Shanghai": [{
        "hash": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
        "indexes": {"data": 0, "gas": 0, "value": 0},
        "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    }]},
    "pre": {},
    "transaction": {
        "data": ["0x"],
        "gasLimit": ["0x00"],
        "gasPrice": "0x0a",
        "nonce": "0x01",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "to": "0x00",
        "value": ["0x00"]
    }
}})";

/// A transaction that succeeds, in a fixture that expects it to be rejected.
constexpr std::string_view UNEXPECTEDLY_VALID = R"({"valid_tx": {
    "env": {
        "currentBaseFee": "0x0a",
        "currentCoinbase": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "currentDifficulty": "0x020000",
        "currentGasLimit": "0xff112233445566",
        "currentNumber": "0x01",
        "currentRandom": "0x0000000000000000000000000000000000000000000000000000000000020000",
        "currentTimestamp": "0x03e8"
    },
    "post": {"Shanghai": [{
        "hash": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
        "indexes": {"data": 0, "gas": 0, "value": 0},
        "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
        "expectException": "TransactionException.NONCE_MISMATCH_TOO_HIGH"
    }]},
    "pre": {"0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
        "nonce": "0x00", "balance": "0xffffffff", "code": "0x", "storage": {}
    }},
    "transaction": {
        "data": ["0x"],
        "gasLimit": ["0x5208"],
        "gasPrice": "0x0a",
        "nonce": "0x00",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "to": "0x0000000000000000000000000000000000000000",
        "value": ["0x00"]
    }
}})";

std::vector<Failure> run(std::string_view fixture)
{
    std::istringstream input{std::string{fixture}};
    const auto tests = load_state_tests(input);

    std::vector<Failure> failures;
    TestReport report{[&](const Failure& failure) { failures.push_back(failure); }};
    evmc::VM vm{evmc_create_evmone()};
    for (const auto& t : tests)
        run_state_test(t, vm, false, report);
    return failures;
}
}  // namespace

TEST(statetest_runner, unexpected_invalid_transactions)
{
    const auto failures = run(NONCE_TOO_HIGH);

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].test, "invalid_nonce");
    EXPECT_EQ(failures[0].where, "Shanghai/0");
    EXPECT_EQ(failures[0].what, "transaction validity");
    EXPECT_EQ(failures[0].detail,
        "unexpected invalid transaction: TransactionException.NONCE_MISMATCH_TOO_HIGH");
}

TEST(statetest_runner, not_rejected_transaction)
{
    const auto failures = run(UNEXPECTEDLY_VALID);

    ASSERT_EQ(failures.size(), 1u);  // It abandons the case, so no state root check follows.
    EXPECT_EQ(failures[0].what, "transaction validity");
    EXPECT_EQ(failures[0].detail, "unexpected valid transaction");
}
