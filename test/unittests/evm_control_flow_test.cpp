// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, jump)
{
    std::string s;
    s += "60be600053";  // m[0] = be
    s += "60fa";        // fa
    s += "60055801";    // PC + 5
    s += "56";          // JUMP
    s += "5050";        // POP x2
    s += "5b";          // JUMPDEST
    s += "600153";      // m[1] = fa
    s += "60026000f3";  // RETURN(0,2)
    execute(44, s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(result.output_size, 2);
    EXPECT_EQ(result.output_data[0], 0xbe);
    EXPECT_EQ(result.output_data[1], 0xfa);
}

TEST_P(evm, jumpi)
{
    std::string s;
    s += "5a600557";      // GAS 5 JUMPI
    s += "00";            // STOP
    s += "5b60016000f3";  // JUMPDEST RETURN(0,1)
    execute(25, s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 0);
}

TEST_P(evm, jumpi_else)
{
    execute(16, dup1(OP_COINBASE) + OP_JUMPI);
    EXPECT_GAS_USED(EVMC_SUCCESS, 15);
    EXPECT_EQ(result.output_size, 0);
}

TEST_P(evm, jumpi_at_the_end)
{
    execute(1000, "5b6001600057");
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
    EXPECT_EQ(gas_used, 1000);
}

TEST_P(evm, bad_jumpdest)
{
    host.tx_context.block_number = 1;
    host.tx_context.block_gas_limit = 0;
    host.tx_context.block_timestamp = 0x80000000;
    for (const auto op : {OP_JUMP, OP_JUMPI})
    {
        execute(bytecode{OP_NUMBER} + OP_GASLIMIT + op);
        EXPECT_EQ(result.status_code, EVMC_BAD_JUMP_DESTINATION);
        EXPECT_EQ(result.gas_left, 0);

        execute(bytecode{OP_NUMBER} + OP_TIMESTAMP + op);
        EXPECT_EQ(result.status_code, EVMC_BAD_JUMP_DESTINATION);
        EXPECT_EQ(result.gas_left, 0);
    }
}

TEST_P(evm, jump_to_block_beginning)
{
    const auto code = jumpi(0, OP_MSIZE) + jump(4);
    execute(code);
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, jumpi_stack)
{
    const auto code = push(0xde) + jumpi(6, calldatasize()) + OP_JUMPDEST + ret_top();
    execute(code);
    EXPECT_OUTPUT_INT(0xde);
    execute(code, "ee"_hex);
    EXPECT_OUTPUT_INT(0xde);
}

TEST_P(evm, jump_over_jumpdest)
{
    // The code contains 2 consecutive JUMPDESTs. The JUMP at the beginning lands on the second one.
    const auto code = push(4) + OP_JUMP + 2 * OP_JUMPDEST;
    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 3 + 8 + 1);
}

TEST_P(evm, jump_to_missing_push_data)
{
    execute(push(5) + OP_JUMP + OP_PUSH1);
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, jump_to_missing_push_data2)
{
    execute(push(6) + OP_JUMP + OP_PUSH2 + "ef");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, jump_dead_code)
{
    execute(push(6) + OP_JUMP + 3 * OP_INVALID + OP_JUMPDEST);
    EXPECT_GAS_USED(EVMC_SUCCESS, 12);
}

TEST_P(evm, stop_dead_code)
{
    execute(OP_STOP + 3 * OP_INVALID + OP_JUMPDEST);
    EXPECT_GAS_USED(EVMC_SUCCESS, 0);
}

TEST_P(evm, dead_code_at_the_end)
{
    execute(OP_STOP + 3 * OP_INVALID);
    EXPECT_GAS_USED(EVMC_SUCCESS, 0);
}

TEST_P(evm, jumpi_jumpdest)
{
    const auto code = calldataload(0) + push(6) + OP_JUMPI + OP_JUMPDEST;

    execute(code, "00"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 20);

    execute(code, "ff"_hex);
    EXPECT_GAS_USED(EVMC_SUCCESS, 20);
}

TEST_P(evm, jumpi_followed_by_stack_underflow)
{
    execute(push(0) + OP_DUP1 + OP_JUMPI + OP_POP);
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, pc_sum)
{
    const auto code = 4 * OP_PC + 3 * OP_ADD + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(6);
}

TEST_P(evm, pc_after_jump_1)
{
    const auto code = push(3) + OP_JUMP + OP_JUMPDEST + OP_PC + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(4);
}

TEST_P(evm, pc_after_jump_2)
{
    const auto code = calldatasize() + push(9) + OP_JUMPI + push(12) + OP_PC + OP_SWAP1 + OP_JUMP +
                      OP_JUMPDEST + OP_GAS + OP_PC + OP_JUMPDEST + ret_top();

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(6);

    execute(code, "ff"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(11);
}
