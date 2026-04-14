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
    const auto code = push(1) + 16 * OP_PUSH0 + bytecode{"e680"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, dupn_end_of_code)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // DUPN at end of code: implicit immediate is 0x00 (valid, decodes to n=145).
    // With 145 items on the stack, DUP145 succeeds.
    const auto code = push(1) + 144 * OP_PUSH0 + bytecode{"e6"};
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, swapn_basic)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = push(2) + 16 * OP_PUSH0 + push(1) + bytecode{"e780"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);
}

TEST_P(evm, exchange_basic)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = push(0) + push(1) + push(2) + bytecode{"e88e"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, dupn_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = 17 * OP_PUSH0 + bytecode{"e65b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, swapn_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = 18 * OP_PUSH0 + bytecode{"e75b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_invalid_immediate)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = 3 * OP_PUSH0 + bytecode{"e852"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_max_m)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // Immediate 0x8f decodes to (n=1, m=29) — the maximum m value.
    // A branchless decode_pair off-by-one would produce (1, 30), causing
    // a spurious stack underflow (regression from Nethermind devnet bug).
    // EXCHANGE swaps stack[1] and stack[29]. After swap, stack[1] has 99.
    // Use SWAP1 to bring it to top for ret_top().
    const auto code = push(99) + 29 * OP_PUSH0 + bytecode{"e88f"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(99);
}

TEST_P(evm, swapn_invalid_immediate_boundaries)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // The invalid range for DUPN/SWAPN is 0x5b..0x7f (91..127).
    // Verify boundary values of the invalid range.
    for (const auto* hex : {"e75b", "e75c", "e75f", "e760", "e77e", "e77f"})
    {
        execute(18 * OP_PUSH0 + bytecode{hex});
        EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
    }
    // Verify adjacent valid values don't trigger UNDEFINED_INSTRUCTION.
    // imm=0x5a decodes to n=235, imm=0x80 decodes to n=17.
    execute(236 * OP_PUSH0 + bytecode{"e75a"});
    EXPECT_STATUS(EVMC_SUCCESS);
    execute(18 * OP_PUSH0 + bytecode{"e780"});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, dupn_stack_overflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // Regression: DUPN overflow with stack at limit (1024) must not cause UB
    // in the stack pointer adjustment (stack_end + stack_height_change).
    const auto code = 1024 * OP_PUSH0 + bytecode{"e680"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, dupn_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 decodes to n=17, but only 16 items on stack.
    const auto code = 16 * OP_PUSH0 + bytecode{"e680"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, swapn_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x80 decodes to n=17, SWAPN needs n+1=18 items but only 17.
    const auto code = 17 * OP_PUSH0 + bytecode{"e780"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, exchange_stack_underflow)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x8e decodes to (n=1, m=2), needs m+1=3 items but only 2.
    const auto code = 2 * OP_PUSH0 + bytecode{"e88e"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, dupn_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // DUPN costs 3 gas. With exactly 2 gas, it should fail with OOG.
    // 17 PUSH0 (2 gas each = 34) + DUPN (3 gas) = 37 total.
    const auto code = 17 * OP_PUSH0 + bytecode{"e680"};
    execute(36, code);  // 1 gas short.
    EXPECT_STATUS(EVMC_OUT_OF_GAS);

    execute(37, code);  // Exact gas.
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, swapn_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    const auto code = 18 * OP_PUSH0 + bytecode{"e780"};
    execute(38, code);  // 18*2=36 for PUSH0 + 3 for SWAPN = 39. Give 38.
    EXPECT_STATUS(EVMC_OUT_OF_GAS);

    execute(39, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, exchange_out_of_gas)
{
    if (is_advanced())
        return;

    rev = EVMC_AMSTERDAM;
    // imm=0x8e decodes to (n=1, m=2), needs 3 items.
    const auto code = 3 * OP_PUSH0 + bytecode{"e88e"};
    execute(8, code);  // 3*2=6 for PUSH0 + 3 for EXCHANGE = 9. Give 8.
    EXPECT_STATUS(EVMC_OUT_OF_GAS);

    execute(9, code);
    EXPECT_STATUS(EVMC_SUCCESS);
}
