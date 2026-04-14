// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-8024: SWAPN, DUPN, EXCHANGE.
/// https://eips.ethereum.org/EIPS/eip-8024

#include "evm_fixture.hpp"
#include <evmone/evmone.h>

using namespace evmone::test;

/// Shorthand for EIP-8024 tests: skip advanced, set Amsterdam revision.
class evm_eip8024 : public evm
{
protected:
    void SetUp() override
    {
        if (is_advanced())
            GTEST_SKIP();
        rev = EVMC_AMSTERDAM;
    }
};

// Use the same VM parameterization as the base `evm` fixture.
// The INSTANTIATE is in evm_fixture.cpp; for a derived class we need our own.
// Since the VMs are internal to evm_fixture.cpp, we instantiate with baseline only
// (advanced is skipped in SetUp anyway).
static evmc::VM baseline{evmc_create_evmone()};
static evmc::VM bnocgoto{evmc_create_evmone(), {{"cgoto", "no"}}};
INSTANTIATE_TEST_SUITE_P(evmone, evm_eip8024, testing::Values(&baseline, &bnocgoto));

// --- DUPN ---

TEST_P(evm_eip8024, dupn_basic)
{
    // imm=0x80 → n=17. Push 17 items, DUP17 duplicates the bottom one.
    const auto code = push(1) + 16 * OP_PUSH0 + bytecode{"e680"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm_eip8024, dupn_end_of_code)
{
    // DUPN at end of code: implicit immediate is 0x00 → n=145.
    const auto code = push(1) + 144 * OP_PUSH0 + bytecode{"e6"};
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm_eip8024, dupn_invalid_immediate)
{
    // 0x5b is in the forbidden range (0x5b..0x7f).
    execute(17 * OP_PUSH0 + bytecode{"e65b"});
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm_eip8024, dupn_stack_overflow)
{
    // Regression: DUPN overflow with stack at limit (1024) must not cause UB
    // in the stack pointer adjustment (stack_end + stack_height_change).
    execute(1024 * OP_PUSH0 + bytecode{"e680"});
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm_eip8024, dupn_stack_underflow)
{
    // imm=0x80 → n=17, but only 16 items on stack.
    execute(16 * OP_PUSH0 + bytecode{"e680"});
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm_eip8024, dupn_out_of_gas)
{
    // 17 PUSH0 (2 gas each = 34) + DUPN (3 gas) = 37 total.
    const auto code = 17 * OP_PUSH0 + bytecode{"e680"};
    execute(36, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(37, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

// --- SWAPN ---

TEST_P(evm_eip8024, swapn_basic)
{
    // imm=0x80 → n=17. SWAP17: swap top with 17th item.
    const auto code = push(2) + 16 * OP_PUSH0 + push(1) + bytecode{"e780"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);
}

TEST_P(evm_eip8024, swapn_invalid_immediate)
{
    // 0x5b is in the forbidden range (0x5b..0x7f).
    execute(18 * OP_PUSH0 + bytecode{"e75b"});
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm_eip8024, swapn_invalid_immediate_boundaries)
{
    // Verify boundary values of the forbidden range (0x5b..0x7f).
    for (const auto* hex : {"e75b", "e75c", "e75f", "e760", "e77e", "e77f"})
    {
        execute(18 * OP_PUSH0 + bytecode{hex});
        EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
    }
    // Adjacent valid values: 0x5a → n=235, 0x80 → n=17.
    execute(236 * OP_PUSH0 + bytecode{"e75a"});
    EXPECT_STATUS(EVMC_SUCCESS);
    execute(18 * OP_PUSH0 + bytecode{"e780"});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm_eip8024, swapn_stack_underflow)
{
    // imm=0x80 → n=17, SWAPN needs n+1=18 items but only 17.
    execute(17 * OP_PUSH0 + bytecode{"e780"});
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm_eip8024, swapn_out_of_gas)
{
    // 18 PUSH0 (36 gas) + SWAPN (3 gas) = 39 total.
    const auto code = 18 * OP_PUSH0 + bytecode{"e780"};
    execute(38, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(39, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

// --- EXCHANGE ---

TEST_P(evm_eip8024, exchange_basic)
{
    // imm=0x8e → (n=1, m=2). Swaps stack[1] and stack[2].
    const auto code = push(0) + push(1) + push(2) + bytecode{"e88e"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm_eip8024, exchange_max_m)
{
    // imm=0x8f → (n=1, m=29), the maximum m value.
    // Regression: branchless decode off-by-one would produce (1, 30).
    const auto code = push(99) + 29 * OP_PUSH0 + bytecode{"e88f"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(99);
}

TEST_P(evm_eip8024, exchange_invalid_immediate)
{
    // 0x52 is the first byte in the forbidden range (0x52..0x7f).
    execute(3 * OP_PUSH0 + bytecode{"e852"});
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm_eip8024, exchange_stack_underflow)
{
    // imm=0x8e → (n=1, m=2), needs m+1=3 items but only 2.
    execute(2 * OP_PUSH0 + bytecode{"e88e"});
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm_eip8024, exchange_out_of_gas)
{
    // 3 PUSH0 (6 gas) + EXCHANGE (3 gas) = 9 total.
    const auto code = 3 * OP_PUSH0 + bytecode{"e88e"};
    execute(8, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(9, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

// --- Regression ---

TEST_P(evm_eip8024, push1_stack_overflow)
{
    // Regression: EIP-8024 stack check bypass must not affect PUSH instructions.
    execute(1025 * push(1));
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}
