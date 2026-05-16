// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>

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
