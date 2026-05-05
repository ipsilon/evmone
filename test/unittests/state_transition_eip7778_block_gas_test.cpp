// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eip7778_sstore_clear_refund_amsterdam)
{
    // EIP-7778: a clearing SSTORE produces a 4800 refund. The receipt exposes
    // it as `gas_refund` so the block accumulates `gas_used + gas_refund`
    // (the pre-refund gas), independent of what the user pays.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    pre[To] = {.storage = {{0x01_bytes32, 0x42_bytes32}}, .code = sstore(1, 0)};

    // Pre-refund: 21000 intrinsic + 5000 (cold SSTORE reset) + 6 (two PUSHes) = 26006.
    // London-schedule clear refund 4800 ≤ cap 26006/5, so the full refund applies.
    expect.gas_used = 26006 - 4800;
    expect.gas_refund = 4800;
    expect.post[To].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x00_bytes32;
}
