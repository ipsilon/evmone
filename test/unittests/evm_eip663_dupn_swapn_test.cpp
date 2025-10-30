// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, dupn)
{
    // DUPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;

    auto pushes = bytecode{};
    for (uint64_t i = 1; i <= 20; ++i)
        pushes += push(i);

    execute(eof_bytecode(pushes + OP_DUPN + "00" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_DUPN + "02" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(18);

    execute(eof_bytecode(pushes + OP_DUPN + "13" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
}

TEST_P(evm, swapn)
{
    // SWAPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;

    auto pushes = bytecode{};
    for (uint64_t i = 1; i <= 20; ++i)
        pushes += push(i);

    execute(eof_bytecode(pushes + OP_SWAPN + "00" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(19);

    execute(eof_bytecode(pushes + OP_SWAPN + "00" + OP_DUPN + "01" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_SWAPN + "01" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(18);

    execute(eof_bytecode(pushes + OP_SWAPN + "01" + OP_DUPN + "02" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_SWAPN + "12" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);

    execute(eof_bytecode(pushes + OP_SWAPN + "12" + OP_DUPN + "13" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);
}

TEST_P(evm, dupn_full_stack)
{
    // DUPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto full_stack_code = bytecode{};
    for (uint64_t i = 1022; i >= 1; --i)
        full_stack_code += push(i);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_DUPN + "00" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_DUPN + "ff" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(257);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_DUPN + "fe" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(256);
}

TEST_P(evm, swapn_full_stack)
{
    // SWAPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto full_stack_code = bytecode{};
    for (uint64_t i = 1023; i >= 1; --i)
        full_stack_code += push(i);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_SWAPN + "00" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(3);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_SWAPN + "ff" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(255 + 3);

    execute(eof_bytecode(full_stack_code + OP_POP + OP_SWAPN + "fe" + ret_top(), 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(254 + 3);

    execute(eof_bytecode(full_stack_code + OP_SWAPN + "ff" + OP_SWAPN + "00" + OP_RETURN, 1023));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(result.output_size, 255 + 2);
}

TEST_P(evm, dupn_dup_consistency)
{
    // DUPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto pushes = bytecode{};
    for (uint64_t i = 32; i >= 1; --i)
        pushes += push(i);

    execute(eof_bytecode(pushes + OP_DUP1 + ret_top(), 34));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);

    execute(eof_bytecode(pushes + OP_DUPN + "00" + ret_top(), 34));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);

    execute(eof_bytecode(pushes + OP_DUP16 + ret_top(), 34));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(16);

    execute(eof_bytecode(pushes + OP_DUPN + "0f" + ret_top(), 34));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(16);
}

TEST_P(evm, swapn_swap_consistency)
{
    // DUPN is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto pushes = bytecode{};
    for (uint64_t i = 32; i >= 1; --i)
        pushes += push(i);

    execute(eof_bytecode(pushes + OP_SWAP1 + ret_top(), 33));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);

    execute(eof_bytecode(pushes + OP_SWAPN + "00" + ret_top(), 33));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(2);

    execute(eof_bytecode(pushes + OP_SWAP16 + ret_top(), 33));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(17);

    execute(eof_bytecode(pushes + OP_SWAPN + "0f" + ret_top(), 33));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(17);
}
