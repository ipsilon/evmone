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
    // `MAX_TX_GAS_LIMIT` (after the NEW_ACCOUNT state-gas refund).
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
    //   where excess_reservoir = gas_limit - intrinsic_regular - intrinsic_state -
    //   (MAX_TX_GAS_LIMIT - intrinsic_regular)
    //                          = gas_limit - MAX_TX_GAS_LIMIT - intrinsic_state
    //                          = 18_000_000 - 16_777_216 - 183_600 = 1_039_184
    //   NEW_ACCOUNT_STATE_GAS = 120 * 1530 = 183_600
    //   raw_gas_used = 18_000_000 - 1_039_184 - 183_600 = 16_777_216 = MAX_TX_GAS_LIMIT
    expect.status = EVMC_FAILURE;
    expect.gas_used = MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;  // No EVM gas refund; identity gas_used + gas_refund == max(R, floor).
    expect.post[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};
}

// A value-bearing CALL to a NON-EXISTENT account charges NEW_ACCOUNT_STATE_GAS
// (120 * 1530 = 183'600) to the state-gas dimension before the sender-balance
// check. When that check light-fails (caller balance < value), no account is
// created and the charge is refilled at the failure boundary (EIP-8037
// source-based refunds, EIPs#11807), so the net state gas (state_block_gas)
// is 0 — the same as the existing-target baseline. The two tests pin this as
// a differential: the ONLY difference is whether the target pre-exists, so
// both regular gas_used and state gas must be identical. If the refill ever
// regresses, the new-account case grows by exactly 183'600 in state gas.
namespace
{
// Gas pinned empirically (Amsterdam: EIP-2780 decomposition + EIP-8037 2D gas).
constexpr int64_t CallLightfailRegularGas = 26'021;
}  // namespace

TEST_F(state_transition, eip8037_call_value_lightfail_new_account_charge_refilled)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    static constexpr auto Target = 0xbeef_address;  // intentionally absent from `pre`

    // To has balance 0, so `CALL value=1` light-fails the sender-balance check —
    // but only AFTER NEW_ACCOUNT_STATE_GAS is charged for the absent Target.
    pre[To] = {.code = call(Target).value(1).gas(0xffff) + OP_STOP};

    expect.status = EVMC_SUCCESS;  // To STOPs after the failed CALL (light failure)
    expect.post[To] = {};          // To survives
    expect.post[Target].exists = false;  // no account was created
    expect.gas_used = CallLightfailRegularGas;
    expect.state_gas = 0;  // the NEW_ACCOUNT charge for the absent Target is refilled
}

TEST_F(state_transition, eip8037_call_value_lightfail_existing_account_baseline)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    static constexpr auto Target = 0xbeef_address;

    pre[To] = {.code = call(Target).value(1).gas(0xffff) + OP_STOP};
    pre[Target] = {.nonce = 1, .code = bytecode{OP_STOP}};  // Target exists → NO new-account charge

    expect.status = EVMC_SUCCESS;
    expect.post[To] = {};
    expect.post[Target] = {.nonce = 1};         // unchanged by the light-failed call
    expect.gas_used = CallLightfailRegularGas;  // same regular gas as the new-account case
    expect.state_gas = 0;                       // Target exists -> no new-account state-gas charge
}
