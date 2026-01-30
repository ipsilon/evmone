// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-8024
/// https://eips.ethereum.org/EIPS/eip-8024

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST(bytecode, eip8024_decode)
{
    EXPECT_EQ(decode(bytecode{"e600"}), "bytecode{} + OP_DUPN + \"00\"");
    EXPECT_EQ(decode(bytecode{"e780"}), "bytecode{} + OP_SWAPN + \"80\"");
    EXPECT_EQ(decode(bytecode{"e6005b"}), "bytecode{} + OP_DUPN + \"00\" + OP_JUMPDEST");
    EXPECT_EQ(decode(bytecode{"e75b"}), "bytecode{} + OP_INVALID_SWAPN + OP_JUMPDEST");
    EXPECT_EQ(decode(bytecode{"e6605b"}), "bytecode{} + OP_INVALID_DUPN + OP_PUSH1 + \"5b\"");
    EXPECT_EQ(decode(bytecode{"e7610000"}),
        "bytecode{} + OP_INVALID_SWAPN + OP_PUSH2 + \"0000\"");
    EXPECT_EQ(decode(bytecode{"e65f"}), "bytecode{} + OP_INVALID_DUPN + OP_PUSH0");
    EXPECT_EQ(decode(bytecode{"e812"}), "bytecode{} + OP_EXCHANGE + \"12\"");
    EXPECT_EQ(decode(bytecode{"e8d0"}), "bytecode{} + OP_EXCHANGE + \"d0\"");
    EXPECT_EQ(decode(bytecode{"e850"}), "bytecode{} + OP_INVALID_EXCHANGE + OP_POP");
}

TEST_P(evm, dupn_basic)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = push(1) + 16 * OP_PUSH0 + bytecode{"e600"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, dupn_implicit_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    // Missing immediate at end of code defaults to 0.
    const auto code = push(1) + 16 * OP_PUSH0 + bytecode{"e6"};
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, swapn_basic)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = push(2) + 16 * OP_PUSH0 + push(1) + bytecode{"e700"} + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);
}

TEST_P(evm, exchange_basic)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = push(0) + push(1) + push(2) + bytecode{"e801"} + OP_SWAP1 + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, dupn_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = 17 * OP_PUSH0 + bytecode{"e65b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, swapn_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = 18 * OP_PUSH0 + bytecode{"e75b"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_invalid_immediate)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = 3 * OP_PUSH0 + bytecode{"e850"};
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, dupn_stack_underflow)
{
    if (is_advanced())
        GTEST_SKIP();
    rev = EVMC_EXPERIMENTAL;
    const auto code = 16 * OP_PUSH0 + bytecode{"e600"};
    execute(code);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}
