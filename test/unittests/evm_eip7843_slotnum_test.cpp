// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-7843: SLOTNUM opcode.
/// https://eips.ethereum.org/EIPS/eip-7843

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, slotnum_values)
{
    rev = EVMC_AMSTERDAM;
    for (const auto slot_number : {0ull, 0x123456789abcdef0ull, 0xffffffffffffffffull})
    {
        host.tx_context.block_slot_number = slot_number;
        execute(OP_SLOTNUM + ret_top());
        EXPECT_STATUS(EVMC_SUCCESS);
        EXPECT_OUTPUT_INT(slot_number);
    }
}

TEST_P(evm, slotnum_gas_cost)
{
    rev = EVMC_AMSTERDAM;
    host.tx_context.block_slot_number = 1;
    execute(bytecode{} + OP_SLOTNUM);
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 2);
}

TEST_P(evm, slotnum_undefined_before_amsterdam)
{
    // SLOTNUM (opcode 0x4b) is introduced in Amsterdam; undefined in earlier forks.
    for (const auto r : {EVMC_FRONTIER, EVMC_OSAKA})
    {
        rev = r;
        execute(bytecode{} + OP_SLOTNUM);
        EXPECT_EQ(result.status_code, EVMC_UNDEFINED_INSTRUCTION) << "fork " << r;
    }
}
