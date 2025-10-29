// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eof1_extdelegatecall_eof1)
{
    rev = EVMC_EXPERIMENTAL;

    constexpr auto callee = 0xca11ee_address;
    pre[callee] = {
        .storage = {{0x02_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(2, 0xcc_bytes32) + mstore(0, 0x010203_bytes32) + ret(0, 32), 2),
    };
    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x02_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + sstore(1, returndataload(0)) + OP_STOP, 3),
    };

    expect.gas_used = 50742;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x010203_bytes32;
    // SSTORE executed on caller.
    expect.post[callee].storage[0x02_bytes32] = 0xdd_bytes32;
    expect.post[*tx.to].storage[0x02_bytes32] = 0xcc_bytes32;
}

TEST_F(state_transition, eof1_extdelegatecall_legacy)
{
    rev = EVMC_EXPERIMENTAL;

    constexpr auto callee = 0xca11ee_address;
    pre[callee] = {
        .code = sstore(3, 0xcc_bytes32),
    };
    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}, {0x02_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            sstore(1, extdelegatecall(callee)) + sstore(2, returndatasize()) + OP_STOP, 3),
    };

    expect.gas_used = 28817;  // Low gas usage because EXTDELEGATECALL fails lightly
    // Call - light failure.
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    // Returndata empty.
    expect.post[*tx.to].storage[0x02_bytes32] = 0x00_bytes32;
    // SSTORE not executed.
    expect.post[callee].storage[0x03_bytes32] = 0x00_bytes32;
    expect.post[*tx.to].storage[0x03_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, extdelegatecall_static)
{
    rev = EVMC_EXPERIMENTAL;
    // Checks if EXTDELEGATECALL forwards the "static" flag.
    constexpr auto callee1 = 0xca11ee01_address;
    constexpr auto callee2 = 0xca11ee02_address;
    pre[callee2] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };
    pre[callee1] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(ret(extdelegatecall(callee2)), 3),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}, {0x02_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            sstore(1, extstaticcall(callee1)) + sstore(2, returndataload(0)) + OP_STOP, 3),
    };
    expect.gas_used = 974995;
    // Outer call - success.
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    // Inner call - abort.
    expect.post[*tx.to].storage[0x02_bytes32] = 0x02_bytes32;
    // SSTORE failed.
    expect.post[callee1].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[callee2].storage[0x01_bytes32] = 0xdd_bytes32;
}

TEST_F(state_transition, extcall_static_with_value)
{
    rev = EVMC_EXPERIMENTAL;

    constexpr auto callee1 = 0xca11ee01_address;
    constexpr auto callee2 = 0xca11ee02_address;
    pre[callee1] = {
        .code = eof_bytecode(extcall(callee2).value(1) + OP_STOP, 4),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage =
            {
                {0x01_bytes32, 0xdd_bytes32},
            },
        .code = eof_bytecode(sstore(1, extstaticcall(callee1)) + OP_STOP, 3),
    };
    expect.gas_used = 989747;
    // Outer call - abort in callee.
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
    expect.post[callee1].exists = true;
}

TEST_F(state_transition, extcall_failing_with_value_balance_check)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee).input(0x0, 0xff).value(0x1)) + OP_STOP, 4),
    };

    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].storage[0x01_bytes32] = 0xdd_bytes32;
}

TEST_F(state_transition, extcall_failing_with_value_additional_cost)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee).input(0x0, 0xff).value(0x1)) + OP_STOP, 4),
    };
    // Fails on value transfer additional cost - maximum gas limit that triggers this
    tx.gas_limit = 37639 - 1;

    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[callee].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extcall_with_value)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .balance = 0x01,
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee).input(0x0, 0xff).value(0x1)) + OP_STOP, 4),
    };
    expect.gas_used = 37845;

    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[callee].storage[0x01_bytes32] = 0xcc_bytes32;
}

TEST_F(state_transition, extcall_min_callee_gas_failure_mode)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_STOP, 0),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee)) + OP_STOP, 4),
    };
    // Just short of what the caller needs + MIN_RETAINED_GAS + MIN_CALLEE_GAS
    tx.gas_limit = 21000 + 4 * 3 + 2600 + 5000 + 2300 - 1;

    // Light failure
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_output)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(0x0a0b_bytes32), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            extcall(callee) + sstore(1, returndatacopy(31, 30, 1) + mload(0)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x000a_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extdelegatecall_output)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(0x0a0b_bytes32), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            extdelegatecall(callee) + sstore(1, returndatacopy(31, 30, 1) + mload(0)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x000a_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extstaticcall_output)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(0x0a0b_bytes32), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            extstaticcall(callee) + sstore(1, returndatacopy(31, 30, 1) + mload(0)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x000a_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_memory)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee).input(0, 0xFFFFFFFF) + OP_STOP, 4),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extdelegatecall_memory)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extdelegatecall(callee).input(0, 0xFFFFFFFF) + OP_STOP, 3),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extstaticcall_memory)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extstaticcall(callee).input(0, 0xFFFFFFFF) + OP_STOP, 3),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extcall_ase_ready_violation)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee =
        0x0000000000000000000000010000000000000000000000000000000000000000_bytes32;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee) + OP_STOP, 4),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_ARGUMENT_OUT_OF_RANGE;
}

TEST_F(state_transition, extdelegatecall_ase_ready_violation)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee =
        0x0000000000000000000000010000000000000000000000000000000000000000_bytes32;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extdelegatecall(callee) + OP_STOP, 3),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_ARGUMENT_OUT_OF_RANGE;
}

TEST_F(state_transition, extstaticcall_ase_ready_violation)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee =
        0x0000000000000000000000010000000000000000000000000000000000000000_bytes32;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extstaticcall(callee) + OP_STOP, 3),
    };
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_ARGUMENT_OUT_OF_RANGE;
}

TEST_F(state_transition, extcall_cold_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee) + OP_STOP, 4),
    };
    tx.gas_limit = 21000 + 4 * 3 + 100 + 2500 - 1;
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extdelegatecall_cold_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extdelegatecall(callee) + OP_STOP, 3),
    };
    tx.gas_limit = 21000 + 3 * 3 + 100 + 2500 - 1;
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extstaticcall_cold_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extstaticcall(callee) + OP_STOP, 3),
    };
    tx.gas_limit = 21000 + 3 * 3 + 100 + 2500 - 1;
    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extcall_value_zero_to_nonexistent_account)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee).value(0x0)) + OP_STOP, 4),
    };
    tx.gas_limit = 21000 + 4 * 3 + 5000 + 2300 + 2600 + 2;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, extcall_then_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + extcall(callee) + rjump(-3), 4),
    };
    // Enough to SSTORE and complete EXTCALL, OOG is sure to be in the infinite loop.
    tx.gas_limit = 1'000'000;

    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[callee].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extdelegatecall_then_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + extdelegatecall(callee) + rjump(-3), 3),
    };
    // Enough to complete EXTDELEGATECALL, OOG in infinite loop.
    tx.gas_limit = 35000;

    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[callee].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extstaticcall_then_oog)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_STOP),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0xcc_bytes32) + extstaticcall(callee) + rjump(-3), 3),
    };
    // Enough to complete EXTSTATICCALL, OOG in infinite loop.
    tx.gas_limit = 35000;

    expect.gas_used = tx.gas_limit;
    expect.post[*tx.to].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_OUT_OF_GAS;
}

TEST_F(state_transition, extcall_callee_revert)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(revert(0, 0), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extdelegatecall_callee_revert)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(revert(0, 0), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extdelegatecall(callee)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extstaticcall_callee_revert)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(revert(0, 0), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extstaticcall(callee)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extcall_callee_abort)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_INVALID),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extcall(callee)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extdelegatecall_callee_abort)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_INVALID),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extdelegatecall(callee)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extstaticcall_callee_abort)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_INVALID),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, extstaticcall(callee)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
    expect.post[callee].exists = true;
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, extcall_input)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(calldataload(0), 0x010203)), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(mstore(0, 0x010203) + extcall(callee).input(0, 32) +
                                 sstore(1, returndataload(0)) + OP_STOP,
            4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extdelegatecall_input)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(calldataload(0), 0x010203)), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(mstore(0, 0x010203) + extdelegatecall(callee).input(0, 32) +
                                 sstore(1, returndataload(0)) + OP_STOP,
            3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extstaticcall_input)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(calldataload(0), 0x010203)), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(mstore(0, 0x010203) + extstaticcall(callee).input(0, 32) +
                                 sstore(1, returndataload(0)) + OP_STOP,
            3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_with_value_enough_gas)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {.balance = 0x1};

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee).value(1) + OP_POP + OP_STOP, 4),
    };

    constexpr auto callee_and_retained = 5000 + 2300;
    // Just enough to ensure callee and retained gas.
    tx.gas_limit = 21000 + 4 * 3 + callee_and_retained + 9000 + 2600 + 2;
    // Callee and retained gas aren't used
    expect.gas_used = tx.gas_limit - callee_and_retained;
    expect.post[*tx.to].exists = true;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_with_value_low_gas)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {.balance = 0x1};

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee).value(1) + OP_POP + OP_STOP, 4),
    };

    // Not enough to ensure MIN_CALLEE_GAS.
    tx.gas_limit = 21000 + 4 * 3 + 9000 + 2600 + 2 - 1;
    expect.gas_used = tx.gas_limit;
    expect.status = EVMC_OUT_OF_GAS;
    expect.post[*tx.to].exists = true;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_recipient_and_code_address)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(OP_ADDRESS, callee) + eq(OP_CALLER, To) + OP_AND), 3),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extcall(callee) + sstore(1, returndataload(0)) + OP_STOP, 4),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extdelegatecall_recipient_and_code_address)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(OP_ADDRESS, To) + eq(OP_CALLER, tx.sender) + OP_AND), 3),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + sstore(1, returndataload(0)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extstaticcall_recipient_and_code_address)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(eq(OP_ADDRESS, callee) + eq(OP_CALLER, To) + OP_AND), 3),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extstaticcall(callee) + sstore(1, returndataload(0)) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].exists = true;
}


TEST_F(state_transition, extcall_value)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .balance = 0x0,
        .code = eof_bytecode(OP_STOP),
    };

    tx.to = To;
    pre[*tx.to] = {
        .balance = 0x1,
        .code = eof_bytecode(extcall(callee).value(0x1) + OP_STOP, 4),
    };
    expect.post[*tx.to].exists = true;
    expect.post[callee].balance = 1;
}

TEST_F(state_transition, returndatasize_before_extcall)
{
    rev = EVMC_EXPERIMENTAL;

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, returndatasize()) + OP_STOP, 2),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, extdelegatecall_returndatasize)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(ret(0, 0x13), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + sstore(1, returndatasize()) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x13_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extdelegatecall_returndatasize_abort)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(OP_INVALID),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + sstore(1, returndatasize()) + OP_STOP, 3),
    };
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, returndatacopy)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;
    const auto call_output =
        0x497f3c9f61479c1cfa53f0373d39d2bf4e5f73f71411da62f1d6b85c03a60735_bytes32;

    pre[callee] = {
        .code = eof_bytecode(ret(call_output), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            extdelegatecall(callee) + sstore(1, returndatacopy(0, 0, 32) + mload(0)) + OP_STOP, 4),
    };
    expect.gas_used = 28654;
    expect.post[*tx.to].storage[0x01_bytes32] = call_output;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, returndataload)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;
    const auto call_output =
        0x497f3c9f61479c1cfa53f0373d39d2bf4e5f73f71411da62f1d6b85c03a60735_bytes32;

    pre[callee] = {
        .code = eof_bytecode(ret(call_output), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + sstore(1, returndataload(0)) + OP_STOP, 3),
    };
    expect.gas_used = 28636;
    expect.post[*tx.to].storage[0x01_bytes32] = call_output;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_clears_returndata)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;
    const auto call_output =
        0x497f3c9f61479c1cfa53f0373d39d2bf4e5f73f71411da62f1d6b85c03a60735_bytes32;

    pre[callee] = {
        .code = eof_bytecode(ret(call_output), 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(
            extcall(callee) + extcall(callee).value(1) + sstore(1, returndatasize()) + OP_STOP, 5),
    };

    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, extcall_gas_refund_propagation)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(sstore(1, 0x00) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(extcall(callee) + OP_STOP, 4),
    };
    expect.gas_used = 21000 + 2600 + 5000 + 6 * 3 - 4800;
    expect.post[*tx.to].exists = true;
    expect.post[callee].storage[0x01_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, extdelegatecall_gas_refund_propagation)
{
    rev = EVMC_EXPERIMENTAL;
    constexpr auto callee = 0xca11ee_address;

    pre[callee] = {
        .code = eof_bytecode(sstore(1, 0x00) + OP_STOP, 2),
    };

    tx.to = To;
    pre[*tx.to] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = eof_bytecode(extdelegatecall(callee) + OP_STOP, 3),
    };
    expect.gas_used = 21000 + 2600 + 5000 + 5 * 3 - 4800;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x00_bytes32;
    expect.post[callee].exists = true;
}
