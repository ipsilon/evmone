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

    // Pre-refund: 15000 intrinsic (EIP-2780: 12000 base + 3000 recipient) + 12100 (cold SSTORE
    // clear: WARM_ACCESS 100 + STORAGE_WRITE 10000 + additional COLD_STORAGE_ACCESS 2000)
    // + 6 (two PUSHes) = 27106. The clear refund is capped at pre-refund/5 = 5421 (EIP-3529).
    expect.gas_used = 27106 - 5421;
    expect.block_gas_used = 27106;
    expect.post[To].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x00_bytes32;
}
