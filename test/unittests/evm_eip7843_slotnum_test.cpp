// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-7843: SLOTNUM opcode.
/// https://eips.ethereum.org/EIPS/eip-7843

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, slotnum_returns_block_slot_number)
{
    rev = EVMC_AMSTERDAM;
    host.tx_context.block_slot_number = 0x123456789abcdef0;

    execute(bytecode{} + OP_SLOTNUM + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    // The 64-bit slot number lives in the low 8 bytes of the 32-byte word
    // (24 zero bytes + 8 bytes of slot number, big-endian).
    EXPECT_EQ(output, "000000000000000000000000000000000000000000000000123456789abcdef0"_hex);
}

TEST_P(evm, slotnum_zero)
{
    rev = EVMC_AMSTERDAM;
    host.tx_context.block_slot_number = 0;

    execute(bytecode{} + OP_SLOTNUM + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_OUTPUT_INT(0);
}

TEST_P(evm, slotnum_max_uint64)
{
    rev = EVMC_AMSTERDAM;
    host.tx_context.block_slot_number = 0xffffffffffffffff;

    execute(bytecode{} + OP_SLOTNUM + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(output, "000000000000000000000000000000000000000000000000ffffffffffffffff"_hex);
}

TEST_P(evm, slotnum_gas_cost_matches_number)
{
    // SLOTNUM is a base-cost opcode (2 gas), the same class as TIMESTAMP /
    // NUMBER / GASLIMIT.  Verify by comparing total gas usage of an otherwise
    // identical program.
    rev = EVMC_AMSTERDAM;
    host.tx_context.block_slot_number = 1;
    host.tx_context.block_number = 1;

    execute(bytecode{} + OP_SLOTNUM + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    const auto slotnum_gas_used = gas_used;

    execute(bytecode{} + OP_NUMBER + ret_top());
    EXPECT_STATUS(EVMC_SUCCESS);
    EXPECT_EQ(gas_used, slotnum_gas_used);
}

TEST_P(evm, slotnum_undefined_before_amsterdam)
{
    if (is_advanced())
        return;  // Advanced codegen recognises Amsterdam opcodes regardless
                 // of the active rev; the fork-gating happens in the
                 // baseline/bnocgoto interpreters.

    // SLOTNUM (opcode 0x4b) is introduced in Amsterdam; earlier forks must
    // treat the opcode as undefined.
    for (auto pre : {EVMC_FRONTIER, EVMC_BYZANTIUM, EVMC_ISTANBUL, EVMC_BERLIN,
             EVMC_LONDON, EVMC_PARIS, EVMC_SHANGHAI, EVMC_CANCUN, EVMC_PRAGUE, EVMC_OSAKA})
    {
        rev = pre;
        execute(bytecode{} + OP_SLOTNUM + ret_top());
        EXPECT_EQ(result.status_code, EVMC_UNDEFINED_INSTRUCTION) << "fork " << pre;
    }
}
