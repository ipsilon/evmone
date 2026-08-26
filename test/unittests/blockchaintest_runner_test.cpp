// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// Tests of what the blockchain test runner reports when a fixture does not hold. Those paths run
/// only when evmone disagrees with a fixture, which a green EEST run never does, so this is the
/// only place they are exercised.

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
/// BlockHeader has no default member initializers, hence the value-initialization.
BlockchainTest one_block_fixture()
{
    BlockchainTest t{};
    t.name = "unit";
    t.network = "Prague";

    auto& g = t.genesis_block_header;
    g.gas_limit = GAS_LIMIT;
    g.transactions_root = state::EMPTY_MPT_HASH;
    g.receipts_root = state::EMPTY_MPT_HASH;
    g.withdrawal_root = state::EMPTY_MPT_HASH;
    g.hash = GENESIS_HASH;

    TestBlock b{};
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

/// The one failure reported for @p what, or a placeholder naming what was reported instead.
Failure only_failure_for(const std::vector<Failure>& failures, std::string_view what)
{
    std::string seen;
    for (const auto& f : failures)
    {
        if (f.what == what)
            return f;
        seen += f.what + "; ";
    }
    return {.what = "no failure for '" + std::string{what} + "', reported: " + seen};
}

std::vector<Failure> run(const BlockchainTest& t)
{
    std::vector<Failure> failures;
    TestReport report{[&](const Failure& failure) { failures.push_back(failure); }};
    evmc::VM vm{evmc_create_evmone()};
    run_blockchain_tests({&t, 1}, vm, report);
    return failures;
}
}  // namespace

TEST(blockchaintest_runner, block_rejections_name_the_rule_that_was_broken)
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
            [](TestBlock& b) { b.block_info.ommers.emplace_back(); },
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

        EXPECT_TRUE(run(t).empty());  // The fixture names the rule evmone rejected it on.
    }
}

TEST(blockchaintest_runner, a_post_state_mismatch_dumps_both_states)
{
    auto t = one_block_fixture();
    t.test_blocks.clear();  // Nothing applied: the post state is the pre-state.
    t.pre_state[0xaa_address] = {.nonce = 1, .balance = 2, .code = bytes{0xfe}};
    t.pre_state[0xaa_address].storage[0x01_bytes32] = 0x02_bytes32;
    t.expectation.post_state = TestState{};  // A different state, so the roots differ.

    const auto f = only_failure_for(run(t), "post state root");
    EXPECT_EQ(f.what, "post state root");
    EXPECT_THAT(f.detail, testing::HasSubstr("Result state:"));
    EXPECT_THAT(f.detail, testing::HasSubstr("Expected state:"));
    // The dump reaches into the account, which is the expensive part it exists for.
    EXPECT_THAT(f.detail, testing::HasSubstr("balance : 0x2"));
    EXPECT_THAT(f.detail, testing::HasSubstr("code : 0xfe"));
    EXPECT_THAT(f.detail, testing::HasSubstr("storage :"));
}

TEST(blockchaintest_runner, a_block_that_is_not_rlp_is_reported_before_it_is_executed)
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

        EXPECT_EQ(only_failure_for(run(t), "block RLP").detail, c.detail);
    }
}
