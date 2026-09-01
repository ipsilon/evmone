// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// Tests of the blockchain test runner's verdicts. EEST reaches validate_block()'s rejections on
/// a green run, but not the reports below them: those need evmone and a fixture to disagree.
/// Neither is reachable from the suites the coverage job runs.

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <test/utils/blockchaintest.hpp>
#include <test/utils/mpt_hash.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace evmc::literals;

namespace
{
constexpr auto GENESIS_HASH = 0x9e11_bytes32;
constexpr int64_t GAS_LIMIT = 0x100000;

/// A fixture with one block that validate_block() accepts, for a test to then break in one way.
BlockchainTest one_block_fixture()
{
    BlockchainTest t;
    t.name = "unit";
    t.network = "Prague";

    auto& g = t.genesis_block_header;
    g.gas_limit = GAS_LIMIT;
    g.transactions_root = state::EMPTY_MPT_HASH;
    g.receipts_root = state::EMPTY_MPT_HASH;
    g.withdrawal_root = state::EMPTY_MPT_HASH;
    g.hash = GENESIS_HASH;

    TestBlock b;
    b.block_info.number = g.block_number + 1;
    b.block_info.parent_hash = g.hash;
    b.block_info.gas_limit = g.gas_limit;
    b.block_info.timestamp = g.timestamp + 1;
    b.block_info.blob_gas_used = 0;  // Both are mandatory from Cancun.
    b.block_info.excess_blob_gas = 0;
    t.test_blocks.push_back(b);

    // A rejected block leaves the chain at genesis and the state as the pre-state.
    t.expectation.last_block_hash = g.hash;
    t.expectation.post_state = mpt_hash(t.pre_state);
    return t;
}

std::vector<Failure> run(const BlockchainTest& t)
{
    std::vector<Failure> failures;
    TestReport report{[&](const Failure& failure) { failures.push_back(failure); }};
    evmc::VM vm{evmc_create_evmone()};
    run_blockchain_test(t, vm, report);
    return failures;
}
}  // namespace

TEST(blockchaintest_runner, block_rejection_rules)
{
    struct Case
    {
        std::string_view what_is_broken;
        void (*brk)(TestBlock&);
        std::string_view exception;
    };
    static constexpr Case CASES[]{
        {"a parent no block has", [](TestBlock& b) { b.block_info.parent_hash = 0xdead_bytes32; },
            "BlockException.UNKNOWN_PARENT"},
        {"a number that does not follow the parent", [](TestBlock& b) { b.block_info.number = 7; },
            "BlockException.INVALID_BLOCK_NUMBER"},
        {"more gas used than the limit allows",
            [](TestBlock& b) { b.block_info.gas_used = b.block_info.gas_limit + 1; },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
        {"a gas limit above the parent's adjustment range",
            [](TestBlock& b) { b.block_info.gas_limit = GAS_LIMIT * 2; },
            "BlockException.INVALID_GASLIMIT"},
        {"a gas limit below the parent's adjustment range",
            [](TestBlock& b) { b.block_info.gas_limit = GAS_LIMIT / 2; },
            "BlockException.INVALID_GASLIMIT"},
        {"a timestamp no newer than the parent's", [](TestBlock& b) { b.block_info.timestamp = 0; },
            "BlockException.INVALID_BLOCK_TIMESTAMP_OLDER_THAN_PARENT"},
        {"a difficulty the parent does not imply",
            [](TestBlock& b) { b.block_info.difficulty = 1; },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
        {"ommers, which merged forks do not have",
            [](TestBlock& b) { b.block_info.ommers.emplace_back(address{}, 1u); },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
        {"no blob gas fields, mandatory from Cancun",
            [](TestBlock& b) { b.block_info.blob_gas_used.reset(); },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
        {"excess blob gas the parent does not imply",
            [](TestBlock& b) { b.block_info.excess_blob_gas = 0x20000; },
            "BlockException.INCORRECT_EXCESS_BLOB_GAS"},
        {"a slot number, which arrives only with Amsterdam",
            [](TestBlock& b) { b.block_info.slot_number = 1; },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
        {"a withdrawal that did not parse",
            [](TestBlock& b) { b.withdrawals_parse_success = false; },
            "BlockException.INCORRECT_BLOCK_FORMAT"},
    };

    for (const auto& c : CASES)
    {
        SCOPED_TRACE(c.what_is_broken);
        auto t = one_block_fixture();
        c.brk(t.test_blocks[0]);
        t.test_blocks[0].expected_exception = c.exception;

        // Matched rather than EXPECT_TRUE(...empty()) so a regression prints what was reported.
        EXPECT_THAT(run(t), testing::IsEmpty());
    }
}

TEST(blockchaintest_runner, post_state_mismatch_dump)
{
    auto t = one_block_fixture();
    t.test_blocks.clear();  // Nothing applied: the post state is the pre-state.
    t.pre_state[0xaa_address] = {.nonce = 1, .balance = 2, .code = bytes{0xfe}};
    t.pre_state[0xaa_address].storage[0x01_bytes32] = 0x02_bytes32;
    t.expectation.post_state = TestState{};  // A different state, so the roots differ.

    const auto failures = run(t);
    ASSERT_EQ(failures.size(), 1u);
    EXPECT_EQ(failures[0].what, "post state root");
    EXPECT_THAT(failures[0].detail, testing::HasSubstr("Result state:"));
    EXPECT_THAT(failures[0].detail, testing::HasSubstr("Expected state:"));
    // Printed last of an account, so reaching it means the whole dump was built.
    EXPECT_THAT(failures[0].detail, testing::HasSubstr("storage :"));
}

TEST(blockchaintest_runner, invalid_block_rlp_reported)
{
    struct Case
    {
        bytes rlp;
        std::string_view detail;
    };
    const Case CASES[]{
        {bytes{0x00}, "not a list"},
        {bytes{0xc0, 0x00}, "trailing bytes after the block"},
        {bytes{0xc1, 0x00}, "the header is not a list"},
        {bytes{0xc1, 0xc0}, "the transactions are not a list"},
    };

    for (const auto& c : CASES)
    {
        SCOPED_TRACE(c.detail);
        auto t = one_block_fixture();
        t.test_blocks[0].rlp = c.rlp;  // Only checked for blocks the fixture expects to be valid.

        const auto failures = run(t);  // The RLP check runs before the block is validated.
        ASSERT_FALSE(failures.empty());
        EXPECT_EQ(failures[0].what, "block RLP");
        EXPECT_EQ(failures[0].detail, c.detail);
    }
}
