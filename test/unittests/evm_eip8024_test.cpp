// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-8024
/// https://eips.ethereum.org/EIPS/eip-8024

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST(bytecode, eip8024_decode)
{
    EXPECT_EQ(decode(bytecode{"e680"}), "bytecode{} + OP_DUPN + \"80\"");
    EXPECT_EQ(decode(bytecode{"e7db"}), "bytecode{} + OP_SWAPN + \"db\"");
    EXPECT_EQ(decode(bytecode{"e6805b"}), "bytecode{} + OP_DUPN + \"80\" + OP_JUMPDEST");
    EXPECT_EQ(decode(bytecode{"e75b"}), "bytecode{} + OP_INVALID_SWAPN + OP_JUMPDEST");
    EXPECT_EQ(decode(bytecode{"e6605b"}), "bytecode{} + OP_INVALID_DUPN + OP_PUSH1 + \"5b\"");
    EXPECT_EQ(decode(bytecode{"e7610000"}),
        "bytecode{} + OP_INVALID_SWAPN + OP_PUSH2 + \"0000\"");
    EXPECT_EQ(decode(bytecode{"e65f"}), "bytecode{} + OP_INVALID_DUPN + OP_PUSH0");
    EXPECT_EQ(decode(bytecode{"e89d"}), "bytecode{} + OP_EXCHANGE + \"9d\"");
    EXPECT_EQ(decode(bytecode{"e82f"}), "bytecode{} + OP_EXCHANGE + \"2f\"");
    EXPECT_EQ(decode(bytecode{"e850"}), "bytecode{} + OP_EXCHANGE + \"50\"");
    EXPECT_EQ(decode(bytecode{"e851"}), "bytecode{} + OP_EXCHANGE + \"51\"");
    EXPECT_EQ(decode(bytecode{"e852"}), "bytecode{} + OP_INVALID_EXCHANGE + OP_MSTORE");
}

TEST_P(evm, dupn_basic)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = push(1) + 16 * OP_PUSH0 + bytecode{"e680"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, dupn_end_of_code)
{
    if (is_advanced())
        GTEST_SKIP();
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
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = push(2) + 16 * OP_PUSH0 + push(1) + bytecode{"e780"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);
}

TEST_P(evm, exchange_basic)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = push(0) + push(1) + push(2) + bytecode{"e88e"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, dupn_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = 17 * OP_PUSH0 + bytecode{"e65b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, swapn_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = 18 * OP_PUSH0 + bytecode{"e75b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = 3 * OP_PUSH0 + bytecode{"e852"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_max_m)
{
    if (is_advanced())
        GTEST_SKIP();
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

TEST_P(evm, dupn_stack_underflow)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_AMSTERDAM;
    const auto code = 16 * OP_PUSH0 + bytecode{"e680"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}
