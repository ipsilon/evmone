// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"

using namespace evmone::test;


TEST_P(evm, exchange)
{
    // EXCHANGE is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;

    auto pushes = bytecode{};
    for (uint64_t i = 1; i <= 20; ++i)
        pushes += push(i);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "00" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "00" + OP_DUPN + "01" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(18);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "00" + OP_DUPN + "02" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(19);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "01" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "01" + OP_DUPN + "01" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(17);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "01" + OP_DUPN + "03" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(19);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "10" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "10" + OP_DUPN + "02" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(17);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "10" + OP_DUPN + "03" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(18);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "f2" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "f2" + OP_DUPN + "10" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "f2" + OP_DUPN + "13" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(4);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "0f" + ret_top(), 21));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(20);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "0f" + OP_DUPN + "01" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(3);

    execute(eof_bytecode(pushes + OP_EXCHANGE + "0f" + OP_DUPN + "11" + ret_top(), 22));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(19);
}

TEST_P(evm, exchange_deep_stack)
{
    // EXCHANGE is not implemented in Advanced.
    if (evm::is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto full_stack_code = bytecode{};
    for (uint64_t i = 255; i >= 1; --i)
        full_stack_code += push(i);

    execute(eof_bytecode(full_stack_code + OP_EXCHANGE + "ff" + ret_top(), 256));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(1);
    execute(eof_bytecode(full_stack_code + OP_EXCHANGE + "ff" + OP_DUPN + "10" + ret_top(), 257));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(33);
    execute(eof_bytecode(full_stack_code + OP_EXCHANGE + "ff" + OP_DUPN + "20" + ret_top(), 257));
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(17);
}
