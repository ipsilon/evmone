// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/evmone.h>
#include <gtest/gtest.h>
#include <test/state/state.hpp>
#include <test/utils/bytecode.hpp>
#include <test/utils/test_state.hpp>

using namespace evmc::literals;
using namespace evmone::state;
using namespace evmone::test;

/// Accessing an account which does not exist warms it up by inserting a temporary empty account
/// (EIP-2929). Reverting the accessing frame must restore the account to non-existent, leaving
/// no trace of it in the state diff.
TEST(state_diff, revert_cold_access_of_nonexistent_account)
{
    static constexpr auto SENDER = 0x5e11de_address;
    static constexpr auto TO = 0xc0de_address;
    static constexpr auto ABSENT = 0xab5e17_address;

    const BlockInfo block{.gas_limit = 1'000'000, .coinbase = 0xc014bace_address};
    const Transaction tx{.gas_limit = block.gas_limit, .sender = SENDER, .to = TO};

    TestState state{
        {SENDER, {.balance = 1'000'000'000}},
        {TO, {.code = push(ABSENT) + OP_BALANCE + OP_POP + revert(0, 0)}},
    };

    evmc::VM vm{evmc_create_evmone()};
    const auto res =
        transition(state, block, TestBlockHashes{}, tx, EVMC_BERLIN, vm, block.gas_limit, 0);

    ASSERT_TRUE(holds_alternative<TransactionReceipt>(res));
    const auto& receipt = std::get<TransactionReceipt>(res);
    ASSERT_EQ(receipt.status, EVMC_REVERT);

    const auto& diff = receipt.state_diff;
    for (const auto& addr : diff.deleted_accounts)
        EXPECT_NE(addr, ABSENT) << "deleted an account which never existed";
    for (const auto& entry : diff.modified_accounts)
        EXPECT_NE(entry.addr, ABSENT) << "modified an account which never existed";
}
