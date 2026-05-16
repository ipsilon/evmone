// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

namespace
{
EngineTest make_empty_engine_test()
{
    EngineTest t;
    t.name = "empty";
    t.network = "Osaka";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.last_block_hash = t.genesis.hash;  // no payloads → head stays at genesis
    return t;
}
}  // namespace

TEST(engine_test_runner, empty_payloads_pass)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_empty_engine_test(), vm);
    EXPECT_TRUE(result.passed) << result.error;
    EXPECT_EQ(result.error, "");
}

namespace
{
EnginePayload make_empty_payload_with_bad_state_root()
{
    EnginePayload p;
    p.block_info.number = 1;
    p.block_info.gas_limit = 0x7270e00;
    p.block_info.gas_used = 0;
    p.block_info.timestamp = 0x3e8;
    p.block_info.base_fee = 7;
    p.block_info.coinbase = 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address;
    p.block_info.parent_hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    p.block_info.blob_gas_used = 0u;
    p.block_info.excess_blob_gas = 0u;
    p.block_info.parent_beacon_block_root =
        0x0000000000000000000000000000000000000000000000000000000000000000_bytes32;
    p.expected_block_hash =
        0x0000000000000000000000000000000000000000000000000000000000000099_bytes32;
    // Deliberately wrong: an empty block produces the empty MPT root for state,
    // not 0xdead...
    p.expected_state_root =
        0xdeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddeaddead_bytes32;
    p.expected_receipts_root =
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_bytes32;
    p.expected_gas_used = 0;
    return p;
}

EngineTest make_test_with_bad_state_root()
{
    EngineTest t;
    t.name = "bad_state_root";
    // Use Cancun (pre-Prague) to avoid the requirement that
    // Prague+ system contracts (withdrawal/consolidation) be pre-deployed.
    t.network = "Cancun";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.payloads.push_back(make_empty_payload_with_bad_state_root());
    t.last_block_hash = t.payloads[0].expected_block_hash;
    return t;
}
}  // namespace

TEST(engine_test_runner, state_root_mismatch_fails)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_test_with_bad_state_root(), vm);
    EXPECT_FALSE(result.passed);
    EXPECT_THAT(result.error, HasSubstr("state root mismatch"));
}

TEST(engine_test_runner, validation_error_with_invalid_block_passes)
{
    auto t = make_test_with_bad_state_root();
    t.payloads[0].validation_error = "BlockException.INVALID_STATE_ROOT";
    // With validation_error set, the state-root mismatch is the expected
    // outcome → result is PASS.
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

TEST(engine_test_runner, validation_error_with_bad_rlp_passes)
{
    auto t = make_test_with_bad_state_root();
    // Replace the (currently empty) RLP list with garbage that will fail
    // decoding before apply_block runs. With validation_error set, the
    // decode failure is the expected outcome → PASS.
    t.payloads[0].transactions_rlp.push_back(*evmc::from_hex("0xff"));
    t.payloads[0].validation_error = "BlockException.INVALID_TRANSACTION";
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(t, vm);
    EXPECT_TRUE(result.passed) << result.error;
}

TEST(engine_test_runner, run_engine_tests_json_prints_pass_for_empty_fixture)
{
    constexpr auto json = R"({
        "minimal_test": {
            "network": "Osaka",
            "lastblockhash": "0x0000000000000000000000000000000000000000000000000000000000000003",
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
                "stateRoot":   "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
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
            "_info": { "fixture-format": "blockchain_test_engine" }
        }
    })";

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_json(json, vm, out);
    EXPECT_EQ(rc, 0);
    const auto s = out.str();
    EXPECT_THAT(s, HasSubstr("PASS minimal_test"));
    EXPECT_THAT(s, HasSubstr("1/1 passed"));
}

TEST(engine_test_runner, e2e_bal711_blake2_delegatecall_passes)
{
    const auto* dir = std::getenv("EVMONE_FIXTURES_DIR");
    if (dir == nullptr)
        GTEST_SKIP() << "EVMONE_FIXTURES_DIR not set; skipping on-disk fixture test";

    const std::filesystem::path path = std::filesystem::path{dir} /
        "blockchain_tests_engine/for_osaka/istanbul/eip152_blake2/blake2_delegatecall/"
        "blake2_precompile_delegatecall.json";
    if (!std::filesystem::exists(path))
        GTEST_SKIP() << "fixture not found: " << path;

    std::ifstream f{path};
    const std::string json{
        std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};

    evmc::VM vm{evmc_create_evmone()};
    std::ostringstream out;
    const int rc = run_engine_tests_json(json, vm, out);
    EXPECT_EQ(rc, 0) << out.str();
}
