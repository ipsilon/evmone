// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eip8037_create_tx_collision_excess_reservoir_refunded)
{
    // EIP-8037 relaxes MAX_TX_GAS_LIMIT at Amsterdam: tx.gas_limit may now
    // exceed 0x1000000 (16 MiB). For a CREATE tx with such a gas_limit, the
    // excess over `MAX_TX_GAS_LIMIT - intrinsic_regular_gas` is placed into
    // the state-gas reservoir (state.cpp:822-839).
    //
    // If the depth-0 CREATE then collides (EIP-7610) with a pre-existing
    // account, Host::create returns failure. The collision branch MUST
    // preserve `result.state_gas_left = msg.state_gas` so the excess
    // reservoir is not silently forfeited by the sender; otherwise the
    // sender is overcharged by the excess amount.
    //
    // This test pins that behavior: a CREATE tx with gas_limit > 16 MiB
    // colliding at depth 0 should bill the sender at most
    // `MAX_TX_GAS_LIMIT` (after the PR #2823 NEW_ACCOUNT*CPSB refund).
    rev = EVMC_AMSTERDAM;

    constexpr int64_t MAX_TX_GAS_LIMIT = 0x1000000;  // 16 MiB
    constexpr int64_t TX_GAS_LIMIT = 18'000'000;     // > MAX_TX_GAS_LIMIT
    static_assert(TX_GAS_LIMIT > MAX_TX_GAS_LIMIT);

    block.gas_limit = TX_GAS_LIMIT * 2;
    tx.gas_limit = TX_GAS_LIMIT;
    // tx.to defaults to nullopt → CREATE tx.

    // SetUp() pre-funded the sender based on the default tx.gas_limit; redo it
    // now that we have bumped it.
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + tx.value + 1;

    // Pre-deploy a contract at the address this CREATE tx would produce so
    // is_create_collision() fires. Sender's default nonce is 1.
    const auto create_address = compute_create_address(Sender, 1);
    pre[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};

    // After the M8 fix:
    //   raw_gas_used = gas_limit - state_gas_left - regular_gas_left
    //               = gas_limit - (excess_reservoir + NEW_ACCOUNT_STATE_GAS) - 0
    //   where excess_reservoir = gas_limit - intrinsic_regular - intrinsic_state - (MAX_TX_GAS_LIMIT - intrinsic_regular)
    //                          = gas_limit - MAX_TX_GAS_LIMIT - intrinsic_state
    //                          = 18_000_000 - 16_777_216 - 183_600 = 1_039_184
    //   NEW_ACCOUNT_STATE_GAS = 120 * 1530 = 183_600
    //   raw_gas_used = 18_000_000 - 1_039_184 - 183_600 = 16_777_216 = MAX_TX_GAS_LIMIT
    expect.status = EVMC_FAILURE;
    expect.gas_used = MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;  // No EVM gas refund; identity gas_used + gas_refund == max(R, floor).
    expect.post[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};
}
