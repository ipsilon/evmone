// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eip7778_sstore_clear_refund_amsterdam)
{
    // EIP-7778: a clearing SSTORE produces a 4800 refund, but the block counts the pre-refund gas
    // independently of what the user pays.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    pre[To] = {.storage = {{0x01_bytes32, 0x42_bytes32}}, .code = sstore(1, 0)};

    // Intrinsic, two PUSHes and the cold SSTORE clear (EIP-8038). The clear refund (11616) is
    // capped at a fifth of the pre-refund gas (EIP-3529).
    constexpr auto PRE_REFUND = 21'000 + 6 + 12'100;
    expect.gas_used = PRE_REFUND - PRE_REFUND / 5;
    expect.block_gas_used = PRE_REFUND;
    expect.post[To].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x00_bytes32;
}
