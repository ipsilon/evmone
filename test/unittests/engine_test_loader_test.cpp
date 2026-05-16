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
