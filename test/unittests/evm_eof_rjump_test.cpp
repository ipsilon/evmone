// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"
#include <evmone/eof.hpp>

using namespace evmone::test;

TEST_P(evm, eof1_rjump)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjumpi(3, 0) + rjump(1) + OP_INVALID + mstore8(0, 1) + ret(0, 1), 2);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code = eof_bytecode(rjumpi(3, 0) + rjump(1) + OP_INVALID + mstore8(0, 1) + ret(0, 1), 2)
               .data("deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_rjump_backward)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjumpi(10, 1) + mstore8(0, 1) + ret(0, 1) + rjump(-13), 2);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    code = eof_bytecode(rjumpi(10, 1) + mstore8(0, 1) + ret(0, 1) + rjump(-13), 2).data("deadbeef");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_rjump_0_offset)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjump(0) + mstore8(0, 1) + ret(0, 1), 2);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_rjumpi)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(
        rjumpi(10, calldataload(0)) + mstore8(0, 2) + ret(0, 1) + mstore8(0, 1) + ret(0, 1), 2);

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 2);
}

TEST_P(evm, eof1_rjumpi_backwards)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjumpi(10, 1) + mstore8(0, 1) + ret(0, 1) +
                                 rjumpi(-16, calldataload(0)) + mstore8(0, 2) + ret(0, 1),
        2);

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 2);
}

TEST_P(evm, eof1_rjumpi_0_offset)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjumpi(0, calldataload(0)) + mstore8(0, 1) + ret(0, 1), 2);

    // RJUMPI condition is true
    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);

    // RJUMPI condition is false
    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 1);
}

TEST_P(evm, eof1_rjumpv_single_offset)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    auto code = eof_bytecode(rjumpv({3}, 0) + OP_JUMPDEST + OP_JUMPDEST + OP_STOP + 20 + 0 + 0 +
                                 OP_DATACOPY + ret(0, 20),
        3)
                    .data("ef000101000402000100010300000000000000fe");

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 20);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000402000100010300000000000000fe"_hex);
}

TEST_P(evm, eof1_rjumpv_multiple_offsets)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const auto code =
        eof_bytecode(rjumpi(12, 1) + 10 + 0 + 0 + OP_DATACOPY + ret(0, 10) +
                         rjumpv({12, -23, 0}, calldataload(0)) + 10 + 10 + 0 + OP_DATACOPY +
                         ret(0, 10) + 20 + 0 + 0 + OP_DATACOPY + ret(0, 20),
            3)
            .data("ef000101000402000100010300000000000000fe");

    execute(code, bytes(31, 0) + "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 10);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "ef000101000402000100"_hex);

    execute(code, bytes(31, 0) + "02"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 10);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "010300000000000000fe"_hex);

    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 20);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size),
        "ef000101000402000100010300000000000000fe"_hex);

    execute(code, bytes(31, 0) + "12"_hex);  // case >= count, same behaviour as for case == 2
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 10);
    EXPECT_EQ(bytes_view(result.output_data, result.output_size), "010300000000000000fe"_hex);
}

TEST_P(evm, eof1_rjumpv_long_jumps)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const auto code_ret_7 = push(7) + ret_top();
    // code_ret_7 and jumpdests together make up 0x7fff bytes
    const auto jumpdests = (0x7fff - static_cast<int>(code_ret_7.size())) * bytecode{OP_JUMPDEST};

    auto code = eof_bytecode(rjumpi(0x7fff, 1) + jumpdests + code_ret_7 +
                                 rjumpv({-0x7fff}, calldataload(0)) + 5 + ret_top(),
        2);

    execute(code, "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(7);

    execute(code, "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(5);

    code =
        eof_bytecode(rjumpv({0x7FFF}, calldataload(0)) + jumpdests + code_ret_7 + 5 + ret_top(), 2);

    execute(code, bytes(31, 0) + "00"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(5);

    execute(code, bytes(31, 0) + "01"_hex);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(7);
}
