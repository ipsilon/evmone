// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-8024: SWAPN, DUPN, EXCHANGE.
/// https://eips.ethereum.org/EIPS/eip-8024

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, dupn_basic)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 → n=17. Push 17 items, DUP17 duplicates the bottom one.
    const auto code = push(1) + 16 * OP_PUSH0 + "e680" + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, dupn_end_of_code)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // DUPN at end of code: implicit immediate is 0x00 → n=145.
    const auto code = push(1) + 144 * OP_PUSH0 + "e6";
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, dupn_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 0x5b is in the forbidden range [0x5b–0x7f].
    execute(17 * OP_PUSH0 + "e65b");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, dupn_stack_overflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // Regression: DUPN overflow with stack at limit (1024) must not cause UB
    // in the stack pointer adjustment (stack_end + stack_height_change).
    execute(1024 * OP_PUSH0 + "e680");
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, dupn_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 → n=17, but only 16 items on stack.
    execute(16 * OP_PUSH0 + "e680");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, dupn_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 17 PUSH0 (2 gas each = 34) + DUPN (3 gas) = 37 total.
    const auto code = 17 * OP_PUSH0 + "e680";
    execute(36, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(37, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, swapn_basic)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 → n=17. SWAP17: swap top with the 17th item.
    const auto code = push(2) + 16 * OP_PUSH0 + push(1) + "e780" + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);
}

TEST_P(evm, swapn_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 0x5b is in the forbidden range [0x5b–0x7f].
    execute(18 * OP_PUSH0 + "e75b");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, swapn_invalid_immediate_boundaries)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // Verify boundary values of the forbidden range [0x5b–0x7f].
    for (const auto* hex : {"e75b", "e75c", "e75f", "e760", "e77e", "e77f"})
    {
        execute(18 * OP_PUSH0 + hex);
        EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
    }
    // Adjacent valid values: 0x5a → n=235, 0x80 → n=17.
    execute(236 * OP_PUSH0 + "e75a");
    EXPECT_STATUS(EVMC_SUCCESS);
    execute(18 * OP_PUSH0 + "e780");
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, swapn_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 → n=17, SWAPN needs n+1=18 items but only 17.
    execute(17 * OP_PUSH0 + "e780");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, swapn_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 18 PUSH0 (36 gas) + SWAPN (3 gas) = 39 total.
    const auto code = 18 * OP_PUSH0 + "e780";
    execute(38, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(39, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, exchange_basic)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x8e → (n=1, m=2). Swaps stack[1] and stack[2].
    const auto code = push(0) + push(1) + push(2) + "e88e" + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, exchange_max_m)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x8f → (n=1, m=29), the maximum m value.
    // Regression: branchless decode off-by-one would produce (1, 30).
    const auto code = push(99) + 29 * OP_PUSH0 + "e88f" + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(99);
}

TEST_P(evm, exchange_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 0x52 is the first byte in the forbidden range [0x52–0x7f].
    execute(3 * OP_PUSH0 + "e852");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x8e → (n=1, m=2), needs m+1=3 items but only 2.
    execute(2 * OP_PUSH0 + "e88e");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, exchange_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // 3 PUSH0 (6 gas) + EXCHANGE (3 gas) = 9 total.
    const auto code = 3 * OP_PUSH0 + "e88e";
    execute(8, code);
    EXPECT_STATUS(EVMC_OUT_OF_GAS);
    execute(9, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, dupn_immediate_0x5b_is_jumpdest)
{
    if (is_advanced())
        return;

    // Code: PUSH1(4) JUMP DUPN 0x5b PUSH1(1) ret_top
    // 0x5b is in the forbidden immediate range, so DUPN is invalid,
    // but JUMPDEST analysis is unchanged: 0x5b IS a valid jump target.
    rev = EVMC_AMSTERDAM;
    execute(push(4) + OP_JUMP + "e65b" + push(1) + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);

    // Pre-Amsterdam: 0xe6 is undefined single-byte opcode, 0x5b is JUMPDEST. Same result.
    rev = EVMC_OSAKA;
    execute(push(4) + OP_JUMP + "e65b" + push(1) + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}
