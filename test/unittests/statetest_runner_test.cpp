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

/// A value transfer which succeeds, spending less than its gas limit and emitting no logs. The
/// coinbase is the recipient and collects no priority fee, so only two accounts end up in the
/// post state.
constexpr std::string_view VALID_TX = R"({"valid_tx": {
    "env": {
        "currentBaseFee": "0x0a",
        "currentCoinbase": "0x0000000000000000000000000000000000000000",
        "currentDifficulty": "0x020000",
        "currentGasLimit": "0xff112233445566",
        "currentNumber": "0x01",
        "currentTimestamp": "0x03e8"
    },
    "post": {"London": [{
        "hash": "0x71322a2754548ef6d3b540e9b6b858379787c2d1f31bc68205b70902c1d6155f",
        "indexes": {"data": 0, "gas": 0, "value": 0},
        "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    }]},
    "pre": {"0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
        "nonce": "0x00", "balance": "0x0de0b6b3a7640000", "code": "0x", "storage": {}
    }},
    "transaction": {
        "data": ["0x"],
        "gasLimit": ["0x061a80"],
        "gasPrice": "0x0a",
        "nonce": "0x00",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "to": "0x0000000000000000000000000000000000000000",
        "value": ["0x01"]
    }
}})";

/// A contract creation whose init code is INVALID: the transaction is valid, but its execution
/// fails and consumes the whole gas limit. Nothing is created, so only the sender ends up in the
/// post state.
constexpr std::string_view FAILING_TX = R"({"failing_tx": {
    "env": {
        "currentBaseFee": "0x0a",
        "currentCoinbase": "0x0000000000000000000000000000000000000000",
        "currentDifficulty": "0x020000",
        "currentGasLimit": "0xff112233445566",
        "currentNumber": "0x01",
        "currentTimestamp": "0x03e8"
    },
    "post": {"London": [{
        "hash": "0x994280467b9327f616db6328828df403c3493f32ca409a86e7ce6a415a7be488",
        "indexes": {"data": 0, "gas": 0, "value": 0},
        "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    }]},
    "pre": {"0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
        "nonce": "0x00", "balance": "0x0de0b6b3a7640000", "code": "0x", "storage": {}
    }},
    "transaction": {
        "data": ["0xfe"],
        "gasLimit": ["0x061a80"],
        "gasPrice": "0x0a",
        "nonce": "0x00",
        "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
        "to": "",
        "value": ["0x00"]
    }
}})";

/// The reproducer from #1490: a value transfer signed by `secretKey`, with no `sender` given.
/// The expected hash is go-ethereum's.
constexpr std::string_view SECRET_KEY_ONLY = R"({"secretkey_sender_recovery": {
    "env": {
        "currentCoinbase": "0x8888f1f195afa192cfee860698584c030f4c9db1",
        "currentNumber": "0x01",
        "currentTimestamp": "0x54c99069",
        "currentGasLimit": "0x2fefd8",
        "currentDifficulty": "0x0",
        "currentRandom": "0x0000000000000000000000000000000000000000000000000000000000000001",
        "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
        "currentBaseFee": "0x1",
        "currentExcessBlobGas": "0x0",
        "withdrawals": []
    },
    "pre": {
        "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
            "code": "0x", "nonce": "0x00", "balance": "0xde0b6b3a7640000", "storage": {}
        },
        "0x095e7baea6a6c7c4c2dfeb977efac326af552d87": {
            "code": "0x", "nonce": "0x00", "balance": "0x00", "storage": {}
        }
    },
    "transaction": {
        "data": ["0x"],
        "gasLimit": ["0x5208"],
        "value": ["0x0"],
        "to": "0x095e7baea6a6c7c4c2dfeb977efac326af552d87",
        "nonce": "0x0",
        "chainId": "0x1",
        "gasPrice": "0x1",
        "secretKey": "0x45a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8"
    },
    "post": {"Cancun": [{
        "indexes": {"data": 0, "gas": 0, "value": 0},
        "hash": "0x5a605f957d4aa4145411d63287ddf1bae2202c30cc6ec86ae003423bb6343778",
        "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    }]}
}})";

/// A case given as raw txbytes, which the runner decodes and recovers the sender of, in a fixture
/// that expects it to succeed. The `transaction` beside it is what the loader wants, not what runs.
std::string txbytes_case(std::string_view txbytes)
{
    return R"({"txbytes_case": {
        "env": {
            "currentCoinbase": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
            "currentGasLimit": "0xff112233445566",
            "currentNumber": "0x01",
            "currentTimestamp": "0x03e8"
        },
        "post": {"Shanghai": [{
            "hash": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "indexes": {"data": 0, "gas": 0, "value": 0},
            "logs": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
            "txbytes": ")" +
           std::string{txbytes} + R"("
        }]},
        "pre": {},
        "transaction": {
            "data": ["0x"], "gasLimit": ["0x5208"], "gasPrice": "0x0a", "nonce": "0x00",
            "value": ["0x00"]
        }
    }})";
}

/// A legacy transaction whose signature `s` is the secp256k1 group order itself, where every
/// valid one is below it.
constexpr auto TXBYTES_INVALID_SIGNATURE =
    "0xeb800a8252088080801b01a0fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141";

/// The failures and the reported output of one run.
struct Run
{
    std::vector<Failure> failures;
    std::string output;
};

Run run(std::string_view fixture, bool trace_summary = false, bool state_diff = false)
{
    std::istringstream input{std::string{fixture}};
    const auto tests = load_state_tests(input);

    std::ostringstream output;
    std::vector<Failure> failures;
    TestReport report{[&](const Failure& failure) { failures.push_back(failure); }};
    evmc::VM vm{evmc_create_evmone()};
    for (const auto& t : tests)
        run_state_test(t, vm,
            {.output = output, .trace_summary = trace_summary, .state_diff = state_diff}, report);
    return {std::move(failures), std::move(output).str()};
}
}  // namespace

TEST(statetest_runner, unexpected_invalid_transactions)
{
    const auto failures = run(NONCE_TOO_HIGH).failures;

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].test, "invalid_nonce");
    EXPECT_EQ(failures[0].where, "Shanghai/0");
    EXPECT_EQ(failures[0].what, "transaction validity");
    EXPECT_EQ(failures[0].detail,
        "unexpected invalid transaction: TransactionException.NONCE_MISMATCH_TOO_HIGH");
}

TEST(statetest_runner, not_rejected_transaction)
{
    // The transaction is valid, but the fixture expects it to be rejected.
    auto fixture = json::json::parse(VALID_TX);
    fixture["valid_tx"]["post"]["London"][0]["expectException"] =
        "TransactionException.NONCE_MISMATCH_TOO_HIGH";

    const auto failures = run(fixture.dump()).failures;

    ASSERT_EQ(failures.size(), 1u);  // It abandons the case, so no state root check follows.
    EXPECT_EQ(failures[0].what, "transaction validity");
    EXPECT_EQ(failures[0].detail, "unexpected valid transaction");
}

TEST(statetest_runner, trace_summary)
{
    const auto [failures, output] = run(VALID_TX, true);

    EXPECT_THAT(failures, testing::IsEmpty());
    EXPECT_EQ(output,
        R"({"pass":true,"gasUsed":"0x5208","logsHash":"0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347","stateRoot":"0x71322a2754548ef6d3b540e9b6b858379787c2d1f31bc68205b70902c1d6155f"})"
        "\n");
}

TEST(statetest_runner, trace_summary_rejected_transaction)
{
    // A rejected transaction has no receipt, so the summary reports only what the state test
    // compares: the hashes, which for a rejection are those of the unchanged pre-state.
    const auto [failures, output] = run(NONCE_TOO_HIGH, true);

    EXPECT_EQ(failures.size(), 1u);
    EXPECT_EQ(output,
        R"({"logsHash":"0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347","stateRoot":"0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421"})"
        "\n");
}

TEST(statetest_runner, trace_summary_failed_transaction)
{
    const auto [failures, output] = run(FAILING_TX, true);

    EXPECT_THAT(failures, testing::IsEmpty());
    EXPECT_EQ(output,
        R"({"pass":false,"error":"invalid instruction","gasUsed":"0x61a80","logsHash":"0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347","stateRoot":"0x994280467b9327f616db6328828df403c3493f32ca409a86e7ce6a415a7be488"})"
        "\n");
}

TEST(statetest_runner, state_diff)
{
    // --state-diff implies --trace-summary, so the diff rides on the same JSON object as the
    // summary rather than replacing it.
    const auto [failures, output] = run(VALID_TX, true, true);

    EXPECT_THAT(failures, testing::IsEmpty());
    EXPECT_EQ(output,
        R"({"pass":true,"gasUsed":"0x5208","logsHash":"0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347","stateRoot":"0x71322a2754548ef6d3b540e9b6b858379787c2d1f31bc68205b70902c1d6155f","stateDiff":{"deletedAccounts":[],"modifiedAccounts":{"0x0000000000000000000000000000000000000000":{"balance":"0x1","modifiedStorage":{},"nonce":"0x0"},"0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b":{"balance":"0xde0b6b3a760cbaf","modifiedStorage":{},"nonce":"0x1"}}}})"
        "\n");
}

TEST(statetest_runner, state_diff_rejected_transaction)
{
    // A rejected transaction has no receipt, so its diff is empty rather than omitted.
    const auto [failures, output] = run(NONCE_TOO_HIGH, true, true);

    EXPECT_EQ(failures.size(), 1u);
    EXPECT_EQ(output,
        R"({"logsHash":"0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347","stateRoot":"0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421","stateDiff":{"deletedAccounts":[],"modifiedAccounts":{}}})"
        "\n");
}

TEST(statetest_runner, trace_summary_output_is_valid_json)
{
    // Verifies the output is proper JSON, regardless of which fields it holds: the pinned tests
    // above catch a wrong value, but not e.g. a trailing comma left over from a conditionally
    // printed field.
    for (const std::string_view fixture : {VALID_TX, NONCE_TOO_HIGH, FAILING_TX})
    {
        for (const bool state_diff : {false, true})
        {
            json::json parsed;
            EXPECT_NO_THROW(parsed = json::json::parse(run(fixture, true, state_diff).output));
            EXPECT_TRUE(parsed.is_object());
        }
    }
}

TEST(statetest_runner, txbytes_invalid_signature)
{
    const auto failures = run(txbytes_case(TXBYTES_INVALID_SIGNATURE)).failures;

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].detail,
        "unexpected invalid transaction: TransactionException.INVALID_SIGNATURE_VRS");
}

TEST(statetest_runner, txbytes_invalid_encoding)
{
    // An empty list, where a legacy transaction is a list of nine items.
    const auto failures = run(txbytes_case("0xc0")).failures;

    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].detail, "unexpected invalid transaction: invalid transaction encoding");
}

TEST(statetest_runner, sender_from_secret_key)
{
    EXPECT_THAT(run(SECRET_KEY_ONLY).failures, testing::IsEmpty());
}
