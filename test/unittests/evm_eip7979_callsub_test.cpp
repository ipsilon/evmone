// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-7979: Call and Return Opcodes for the EVM.
/// https://eips.ethereum.org/EIPS/eip-7979

#include "evm_fixture.hpp"

using namespace evmone::test;

namespace
{
constexpr auto REV = EVMC_EXPERIMENTAL;
}

TEST_P(evm, callsub_undefined_before_experimental)
{
    rev = EVMC_AMSTERDAM;
    execute("6004B000B1B2");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, calldest_not_a_jumpdest_before_experimental)
{
    if (is_advanced())
        return;
    rev = EVMC_AMSTERDAM;
    // PUSH1 4, JUMP, STOP, 0xB1, STOP: before EIP-7979 a 0xB1 byte is not a jump destination.
    execute("60045600B100");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

// The test cases of the EIP.

TEST_P(evm, eip7979_simple_routine)
{
    if (is_advanced())
        return;
    rev = REV;
    // PUSH1 4, CALLSUB, STOP, CALLDEST, RETURNSUB
    execute("6004B000B1B2");
    EXPECT_GAS_USED(EVMC_SUCCESS, 17);
}

TEST_P(evm, eip7979_two_levels_of_subroutines)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("6004B000B16009B0B2B1B2");
    EXPECT_GAS_USED(EVMC_SUCCESS, 34);
}

TEST_P(evm, eip7979_invalid_destination)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("60FFB000B1B2");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, eip7979_empty_return_stack)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("B2");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, eip7979_subroutine_at_end_of_code)
{
    if (is_advanced())
        return;
    rev = REV;
    // PUSH1 5, JUMP, CALLDEST, RETURNSUB, JUMPDEST, PUSH1 3, CALLSUB: returns to the implicit STOP.
    execute("600556B1B25B6003B0");
    EXPECT_GAS_USED(EVMC_SUCCESS, 29);
}

// Destinations.

TEST_P(evm, callsub_to_jumpdest)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("6004B0005B");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, callsub_into_push_data)
{
    if (is_advanced())
        return;
    rev = REV;
    // Destination 5 is the 0xB1 byte inside the PUSH1 immediate.
    execute("6005B00060B100");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, callsub_destination_overflows_uint64)
{
    if (is_advanced())
        return;
    rev = REV;
    execute(push(~intx::uint256{}) + "B000B1B2");
    EXPECT_STATUS(EVMC_BAD_JUMP_DESTINATION);
}

TEST_P(evm, callsub_stack_underflow)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("B000B1B2");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

// CALLDEST is a valid JUMP/JUMPI destination (call elimination).

TEST_P(evm, jump_to_calldest)
{
    if (is_advanced())
        return;
    rev = REV;
    // PUSH1 4, JUMP, STOP, CALLDEST, STOP
    execute("60045600B100");
    EXPECT_GAS_USED(EVMC_SUCCESS, 12);
}

TEST_P(evm, jumpi_to_calldest)
{
    if (is_advanced())
        return;
    rev = REV;
    // PUSH1 1, PUSH1 6, JUMPI, STOP, CALLDEST, STOP
    execute("600160065700B100");
    EXPECT_GAS_USED(EVMC_SUCCESS, 17);
}

TEST_P(evm, returnsub_after_unframed_jump)
{
    if (is_advanced())
        return;
    rev = REV;
    // Entering a subroutine by JUMP pushes no return address.
    execute("60045600B1B2");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}

TEST_P(evm, tail_call)
{
    if (is_advanced())
        return;
    rev = REV;
    // main: PUSH1 4, CALLSUB, STOP; f: CALLDEST, PUSH1 8, JUMP; g: CALLDEST, RETURNSUB
    execute("6004B000B1600856B1B2");
    EXPECT_GAS_USED(EVMC_SUCCESS, 29);
}

TEST_P(evm, calldest_reached_by_fall_through)
{
    if (is_advanced())
        return;
    rev = REV;
    execute("B1B100");
    EXPECT_GAS_USED(EVMC_SUCCESS, 2);
}

// The return stack.

TEST_P(evm, subroutine_returns_value_to_caller)
{
    if (is_advanced())
        return;
    rev = REV;
    // main: PUSH1 <sub>, CALLSUB, then return the value the subroutine left on the stack.
    // sub: CALLDEST, PUSH1 0x2a, RETURNSUB
    const auto main = push(0) + OP_CALLSUB + ret_top();
    const auto code =
        push(main.size()) + OP_CALLSUB + ret_top() + OP_CALLDEST + push(0x2a) + OP_RETURNSUB;
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0x2a);
}

TEST_P(evm, return_stack_overflow)
{
    if (is_advanced())
        return;
    rev = REV;
    // Recursion with no base case: PUSH1 4, CALLSUB, STOP; CALLDEST, PUSH1 4, CALLSUB, RETURNSUB
    execute("6004B000B16004B0B2");
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, return_stack_limit)
{
    if (is_advanced())
        return;
    rev = REV;
    // Counted recursion `depth` levels deep; the return stack holds depth + 1 entries.
    //   main: PUSH2 depth, PUSH1 sub, CALLSUB, STOP                       (7 bytes)
    //   sub:  CALLDEST, DUP1, ISZERO, PUSH1 done, JUMPI,                  (6 bytes)
    //         PUSH1 1, SWAP1, SUB, PUSH1 sub, CALLSUB, RETURNSUB,        (8 bytes)
    //   done: JUMPDEST, RETURNSUB                                          (2 bytes)
    const auto run = [&](uint64_t depth) {
        constexpr uint64_t sub_pos = 7;
        constexpr uint64_t done_pos = sub_pos + 14;
        // push(depth) is a PUSH2 for both depths tested, so main is 7 bytes.
        const auto code = push(depth) + push(sub_pos) + OP_CALLSUB + OP_STOP +             //
                          OP_CALLDEST + OP_DUP1 + OP_ISZERO + push(done_pos) + OP_JUMPI +  //
                          push(1) + OP_SWAP1 + OP_SUB + push(sub_pos) + OP_CALLSUB +
                          OP_RETURNSUB +  //
                          OP_JUMPDEST + OP_RETURNSUB;
        execute(1'000'000, code);
    };
    run(1023);
    EXPECT_STATUS(EVMC_SUCCESS);
    run(1024);
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
}

TEST_P(evm, return_stack_reset_between_executions)
{
    if (is_advanced())
        return;
    rev = REV;
    // A frame that halts with return addresses pending leaves nothing behind for the
    // next execution: the return stack belongs to the frame.
    execute("6004B000B16004B0B2");
    EXPECT_STATUS(EVMC_STACK_OVERFLOW);
    execute("B2");
    EXPECT_STATUS(EVMC_STACK_UNDERFLOW);
}
