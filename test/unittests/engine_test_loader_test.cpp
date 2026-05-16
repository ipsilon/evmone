// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

// A minimal-but-valid blockchain_test_engine fixture with zero payloads.
constexpr auto MINIMAL_ENGINE_JSON = R"({
    "minimal_test": {
        "network": "Osaka",
        "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000001",
        "config": {
            "network": "Osaka",
            "chainid": "0x01",
            "blobSchedule": {
                "Osaka": {
                    "target": "0x06",
                    "max": "0x09",
                    "baseFeeUpdateFraction": "0x4c6964"
                }
            }
        },
        "pre": {},
        "genesisBlockHeader": {
            "parentHash":  "0x0000000000000000000000000000000000000000000000000000000000000000",
            "uncleHash":   "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
            "coinbase":    "0x0000000000000000000000000000000000000000",
            "stateRoot":   "0x0000000000000000000000000000000000000000000000000000000000000002",
            "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
            "difficulty": "0x00",
            "number": "0x00",
            "gasLimit": "0x07270e00",
            "gasUsed": "0x00",
            "timestamp": "0x00",
            "extraData": "0x00",
            "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "nonce": "0x0000000000000000",
            "baseFeePerGas": "0x07",
            "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "blobGasUsed": "0x00",
            "excessBlobGas": "0x00",
            "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "requestsHash": "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "hash": "0x0000000000000000000000000000000000000000000000000000000000000003"
        },
        "engineNewPayloads": [],
        "postState": {},
        "_info": {
            "hash": "0xaa",
            "fixture-format": "blockchain_test_engine"
        }
    }
})";

TEST(engine_test_loader, minimal_fixture)
{
    const auto tests = load_engine_tests(MINIMAL_ENGINE_JSON);
    ASSERT_EQ(tests.size(), 1u);
    EXPECT_EQ(tests[0].name, "minimal_test");
    EXPECT_EQ(tests[0].network, "Osaka");
    EXPECT_TRUE(tests[0].pre_state.empty());
    EXPECT_TRUE(tests[0].post_state.empty());
    EXPECT_EQ(tests[0].payloads.size(), 0u);
    EXPECT_EQ(tests[0].last_block_hash,
        0x0000000000000000000000000000000000000000000000000000000000000001_bytes32);
    EXPECT_EQ(tests[0].genesis.block_number, 0);
    EXPECT_EQ(tests[0].genesis.hash,
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32);
}

TEST(engine_test_loader, rejects_blockchain_test_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "blockchain_test" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_state_test_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "state_test" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_blockchain_test_engine_x_format)
{
    constexpr auto json = R"({
        "t": { "_info": { "fixture-format": "blockchain_test_engine_x" } }
    })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

TEST(engine_test_loader, rejects_missing_info)
{
    constexpr auto json = R"({ "t": { "network": "Osaka" } })";
    EXPECT_THROW(load_engine_tests(json), UnsupportedTestFeature);
}

// A blockchain_test_engine fixture with one transaction-free payload that
// also carries a validationError. The payload has minimal but valid fields.
constexpr auto SINGLE_PAYLOAD_JSON = R"({
    "single_payload": {
        "network": "Osaka",
        "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000005",
        "config": {
            "network": "Osaka",
            "chainid": "0x01",
            "blobSchedule": {
                "Osaka": {
                    "target": "0x06",
                    "max": "0x09",
                    "baseFeeUpdateFraction": "0x4c6964"
                }
            }
        },
        "pre": {},
        "postState": {},
        "genesisBlockHeader": {
            "parentHash":  "0x0000000000000000000000000000000000000000000000000000000000000000",
            "uncleHash":   "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
            "coinbase":    "0x0000000000000000000000000000000000000000",
            "stateRoot":   "0x0000000000000000000000000000000000000000000000000000000000000002",
            "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
            "difficulty": "0x00",
            "number": "0x00",
            "gasLimit": "0x07270e00",
            "gasUsed": "0x00",
            "timestamp": "0x00",
            "extraData": "0x00",
            "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "nonce": "0x0000000000000000",
            "baseFeePerGas": "0x07",
            "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
            "blobGasUsed": "0x00",
            "excessBlobGas": "0x00",
            "parentBeaconBlockRoot": "0x0000000000000000000000000000000000000000000000000000000000000000",
            "requestsHash": "0xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "hash": "0x0000000000000000000000000000000000000000000000000000000000000003"
        },
        "engineNewPayloads": [
            {
                "params": [
                    {
                        "parentHash":     "0x0000000000000000000000000000000000000000000000000000000000000003",
                        "feeRecipient":   "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
                        "stateRoot":      "0x0000000000000000000000000000000000000000000000000000000000000020",
                        "receiptsRoot":   "0x0000000000000000000000000000000000000000000000000000000000000021",
                        "logsBloom":      "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "blockNumber":    "0x1",
                        "gasLimit":       "0x7270e00",
                        "gasUsed":        "0x534a",
                        "timestamp":      "0x3e8",
                        "extraData":      "0x00",
                        "prevRandao":     "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "baseFeePerGas":  "0x7",
                        "blobGasUsed":    "0x0",
                        "excessBlobGas":  "0x0",
                        "blockHash":      "0x0000000000000000000000000000000000000000000000000000000000000099",
                        "transactions": [
                            "0xdeadbeef"
                        ],
                        "withdrawals": []
                    },
                    [],
                    "0x0000000000000000000000000000000000000000000000000000000000000010",
                    []
                ],
                "newPayloadVersion": "4",
                "forkchoiceUpdatedVersion": "3",
                "validationError": "BlockException.INVALID_BASEFEE_PER_GAS"
            }
        ],
        "_info": {
            "hash": "0xbb",
            "fixture-format": "blockchain_test_engine"
        }
    }
})";

TEST(engine_test_loader, single_payload_with_validation_error)
{
    const auto tests = load_engine_tests(SINGLE_PAYLOAD_JSON);
    ASSERT_EQ(tests.size(), 1u);
    const auto& t = tests[0];
    ASSERT_EQ(t.payloads.size(), 1u);

    const auto& p = t.payloads[0];

    // BlockInfo mapping
    EXPECT_EQ(p.block_info.number, 1);
    EXPECT_EQ(p.block_info.gas_limit, 0x7270e00);
    EXPECT_EQ(p.block_info.gas_used, 0x534a);
    EXPECT_EQ(p.block_info.timestamp, 0x3e8);
    EXPECT_EQ(p.block_info.base_fee, 0x7u);
    EXPECT_EQ(p.block_info.coinbase,
        0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(p.block_info.parent_hash,
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32);
    EXPECT_EQ(p.block_info.parent_beacon_block_root,
        0x0000000000000000000000000000000000000000000000000000000000000010_bytes32);
    EXPECT_TRUE(p.block_info.blob_gas_used.has_value());
    EXPECT_EQ(*p.block_info.blob_gas_used, 0u);
    EXPECT_TRUE(p.block_info.excess_blob_gas.has_value());
    EXPECT_EQ(*p.block_info.excess_blob_gas, 0u);
    EXPECT_TRUE(p.block_info.withdrawals.empty());

    // Transactions stored as raw RLP bytes, no decode.
    ASSERT_EQ(p.transactions_rlp.size(), 1u);
    EXPECT_EQ(p.transactions_rlp[0], evmc::from_hex("0xdeadbeef").value());

    // Expected outputs
    EXPECT_EQ(p.expected_block_hash,
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32);
    EXPECT_EQ(p.expected_state_root,
        0x0000000000000000000000000000000000000000000000000000000000000020_bytes32);
    EXPECT_EQ(p.expected_receipts_root,
        0x0000000000000000000000000000000000000000000000000000000000000021_bytes32);
    EXPECT_EQ(p.expected_gas_used, 0x534a);

    // validation_error propagated.
    ASSERT_TRUE(p.validation_error.has_value());
    EXPECT_EQ(*p.validation_error, "BlockException.INVALID_BASEFEE_PER_GAS");
}
