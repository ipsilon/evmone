// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"
#include <evmone/eof.hpp>

using namespace evmone::test;

TEST_P(evm, eof_function_example1)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(push(1) + push(8) + OP_CALLF + "0001" + ret_top(), 2)
                              .code(bytecode{OP_SUB} + OP_RETF, 2, 1);

    execute(code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 32);
    EXPECT_OUTPUT_INT(7);
}

TEST_P(evm, eof_function_example2)
{
    // Relative jumps are not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const auto code =
        "ef0001 01000c 020003 003b 0017 001d FF0000 00 00800004 01010002 01010003"
        "60043560003560e01c63c76652678114e1001c63c6c2ea178114e100065050600080fd50e30002600052602060"
        "00f350e3000160005260206000f3"
        "60018111e10004506001e460018103e3000181029050e4"
        "60028111e10004506001e460028103e3000260018203e30002019050e4"_hex;

    // Call fac(5)
    const auto calldata_fac =
        "c76652670000000000000000000000000000000000000000000000000000000000000005"_hex;
    execute(bytecode{code}, calldata_fac);
    EXPECT_GAS_USED(EVMC_SUCCESS, 246);
    EXPECT_EQ(output, "0000000000000000000000000000000000000000000000000000000000000078"_hex);

    // Call fib(15)
    const auto calldata_fib =
        "c6c2ea17000000000000000000000000000000000000000000000000000000000000000f"_hex;
    execute(bytecode{code}, calldata_fib);
    EXPECT_GAS_USED(EVMC_SUCCESS, 44544);
    EXPECT_EQ(output, "0000000000000000000000000000000000000000000000000000000000000262"_hex);
}

TEST_P(evm, callf_stack_size_1024)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push(1) + OP_CALLF + "0001" + 1021 * OP_POP + OP_RETURN, 1023)
            .code(push(1) + OP_POP + OP_RETF, 0, 0, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, callf_with_inputs_stack_size_1024)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push(1) + OP_CALLF + "0001" + 1021 * OP_POP + OP_RETURN, 1023)
            .code(push(1) + OP_POP + OP_RETF, 3, 3, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, callf_stack_overflow)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push(1) + OP_CALLF + "0001" + 1021 * OP_POP + OP_RETURN, 1023)
            .code(push(1) + OP_CALLF + "0002" + OP_POP + OP_RETF, 0, 0, 1)
            .code(push(1) + OP_POP + OP_RETF, 0, 0, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, callf_with_inputs_stack_overflow)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push(1) + OP_CALLF + "0001" + 1021 * OP_POP + OP_RETURN, 1023)
            .code(push(1) + OP_CALLF + "0002" + OP_POP + OP_RETF, 3, 3, 1)
            .code(push(1) + OP_POP + OP_RETF, 3, 3, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, callf_call_stack_size_1024)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(push(1023) + OP_CALLF + "0001" + OP_STOP, 1)
                              .code(bytecode{OP_DUP1} + OP_RJUMPI + "0002" + OP_POP + OP_RETF +
                                        push(1) + OP_SWAP1 + OP_SUB + OP_CALLF + "0001" + OP_RETF,
                                  1, 0, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, callf_call_stack_size_1025)
{
    // CALLF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(push(1024) + OP_CALLF + "0001" + OP_STOP, 1)
                              .code(bytecode{OP_DUP1} + OP_RJUMPI + "0002" + OP_POP + OP_RETF +
                                        push(1) + OP_SWAP1 + OP_SUB + OP_CALLF + "0001" + OP_RETF,
                                  1, 0, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, minimal_jumpf)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(bytecode{OP_JUMPF} + "0001").code(bytecode{OP_STOP}, 0, 0x80);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, jumpf_to_returning_function)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(
        bytecode{OP_CALLF} + "0001" + OP_PUSH0 + OP_MSTORE + OP_PUSH1 + "20" + OP_PUSH0 + OP_RETURN,
        2)
                              .code(bytecode{OP_PUSH1} + "01" + OP_JUMPF + "0002", 0, 1, 1)
                              .code(bytecode{OP_PUSH1} + "02" + OP_ADD + OP_RETF, 1, 1, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(3);
}

TEST_P(evm, jumpf_to_function_with_fewer_outputs)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(
        bytecode{OP_CALLF} + "0001" + OP_PUSH0 + OP_MSTORE + OP_PUSH1 + "20" + OP_PUSH0 + OP_RETURN,
        3)
                              .code(push(0xff) + push(0x01) + OP_JUMPF + "0002", 0, 2, 2)
                              .code(push(0x02) + OP_ADD + OP_RETF, 1, 1, 1);

    execute(bytecode{code});
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(3);
}

TEST_P(evm, jumpf_stack_size_1024)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push0() + OP_JUMPF + "0001", 1023).code(push0() + OP_STOP, 0, 0x80, 1);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, jumpf_with_inputs_stack_size_1024)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code =
        eof_bytecode(1023 * push0() + OP_JUMPF + "0001", 1023).code(push0() + OP_STOP, 3, 0x80, 1);

    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
}

TEST_P(evm, jumpf_stack_overflow)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(1023 * push0() + OP_JUMPF + "0001", 1023)
                              .code(push0() + OP_JUMPF + "0002", 0, 0x80, 1)
                              .code(push0() + OP_STOP, 0, 0x80, 1);

    execute(code);
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, jumpf_with_inputs_stack_overflow)
{
    // JUMPF is not implemented in Advanced.
    if (is_advanced())
        return;

    rev = EVMC_EXPERIMENTAL;
    const bytecode code = eof_bytecode(1023 * push0() + OP_JUMPF + "0001", 1023)
                              .code(push0() + OP_JUMPF + "0002", 3, 0x80, 1)
                              .code(push0() + OP_STOP, 3, 0x80, 1);

    ASSERT_EQ(evmone::validate_eof(rev, evmone::ContainerKind::runtime, code),
        evmone::EOFValidationError::success);
    execute(code);
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}
