// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <evmone/constants.hpp>
#include <evmone/instructions_traits.hpp>
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eip8037_create_tx_collision_excess_reservoir_refunded)
{
    // Amsterdam lets tx.gas_limit exceed MAX_TX_GAS_LIMIT, placing the excess in the state-gas
    // reservoir. A depth-0 CREATE that collides (EIP-7610) must return that reservoir rather
    // than forfeit it, so the sender is billed at most MAX_TX_GAS_LIMIT.
    rev = EVMC_AMSTERDAM;

    constexpr int64_t TX_GAS_LIMIT = 18'000'000;
    static_assert(TX_GAS_LIMIT > state::MAX_TX_GAS_LIMIT);

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

    // The collision returns before the NEW_ACCOUNT charge, so no state gas is charged:
    //   reservoir    = (gas_limit - intrinsic) - (MAX_TX_GAS_LIMIT - intrinsic)
    //                = 18'000'000 - 16'777'216 = 1'222'784
    //   raw_gas_used = gas_limit - gas_left(0) - reservoir = MAX_TX_GAS_LIMIT
    expect.status = EVMC_FAILURE;
    expect.gas_used = state::MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;  // No EVM gas refund; identity gas_used + gas_refund == max(R, floor).
    expect.post[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};
}

// A value-bearing CALL to a NON-EXISTENT account charges NEW_ACCOUNT_STATE_GAS
// (120 * 1530 = 183'600) to the state-gas dimension before the sender-balance
// check. When that check light-fails (caller balance < value), no account is
// created and the charge is refilled at the failure boundary (EIP-8037
// source-based refunds), so the net state gas (block_state_gas)
// is 0 — the same as the existing-target baseline. The two tests pin this as
// a differential: the ONLY difference is whether the target pre-exists, so
// both regular gas_used and state gas must be identical. If the refill ever
// regresses, the new-account case grows by exactly 183'600 in state gas.
namespace
{
// Gas pinned empirically: 21000 intrinsic + the CALL's regular cost, with the
// EIP-8037 NEW_ACCOUNT state charge refilled on the light failure.
constexpr int64_t CALL_LIGHTFAIL_REGULAR_GAS = 30'321;
}  // namespace

TEST_F(state_transition, eip8037_call_value_lightfail_new_account_charge_refilled)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    static constexpr auto TARGET = 0xbeef_address;  // intentionally absent from `pre`

    // To has balance 0, so `CALL value=1` light-fails the sender-balance check —
    // but only AFTER NEW_ACCOUNT_STATE_GAS is charged for the absent TARGET.
    pre[To] = {.code = call(TARGET).value(1).gas(0xffff) + OP_STOP};

    expect.status = EVMC_SUCCESS;        // To STOPs after the failed CALL (light failure)
    expect.post[To] = {};                // To survives
    expect.post[TARGET].exists = false;  // no account was created
    expect.gas_used = CALL_LIGHTFAIL_REGULAR_GAS;
    expect.state_gas = 0;  // the NEW_ACCOUNT charge for the absent TARGET is refilled
}

TEST_F(state_transition, eip8037_call_value_lightfail_existing_account_baseline)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    static constexpr auto TARGET = 0xbeef_address;

    pre[To] = {.code = call(TARGET).value(1).gas(0xffff) + OP_STOP};
    pre[TARGET] = {.nonce = 1, .code = bytecode{OP_STOP}};  // TARGET exists → NO new-account charge

    expect.status = EVMC_SUCCESS;
    expect.post[To] = {};
    expect.post[TARGET] = {.nonce = 1};            // unchanged by the light-failed call
    expect.gas_used = CALL_LIGHTFAIL_REGULAR_GAS;  // same regular gas as the new-account case
    expect.state_gas = 0;  // TARGET exists -> no new-account state-gas charge
}

TEST_F(state_transition, eip8037_value_to_zero_balance_precompile_pays_new_account)
{
    // Funding a zero-balance precompile at depth 0 materializes a state account, so it pays
    // NEW_ACCOUNT_STATE_GAS (EIP-161). The reservoir is empty for a below-cap gas limit, so the
    // whole charge spills into regular gas and the precompile must run on the post-charge gas.
    rev = EVMC_AMSTERDAM;
    tx.to = 0x04_address;  // identity, intentionally absent from `pre`
    tx.value = 1;

    static constexpr int64_t IDENTITY_BASE_COST = 15;

    expect.status = EVMC_SUCCESS;
    expect.post[*tx.to].balance = 1;
    expect.gas_used = 21'000 + IDENTITY_BASE_COST + evmone::NEW_ACCOUNT_STATE_GAS;
    expect.state_gas = evmone::NEW_ACCOUNT_STATE_GAS;
}

TEST_F(state_transition, eip8037_sstore_slot_allocated_and_cleared_in_one_tx)
{
    // Allocating a storage slot and clearing it in the same transaction (0 -> 1 -> 0) refills
    // the STORAGE_SET_STATE_GAS charge, leaving the net state gas at zero.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    pre[To] = {.code = sstore(1, 1) + sstore(1, 0)};

    // Pre-refund: 21000 intrinsic + 12 (four PUSHes) + 5000 (cold slot allocation)
    // + 100 (warm clear) = 26112. The clear refunds set - warm_access = 2800.
    expect.gas_used = 26112 - 2800;
    expect.gas_refund = 2800;
    expect.state_gas = 0;
    expect.post[To].exists = true;
}

TEST_F(state_transition, eip8037_sstore_slot_cleared_in_a_child_frame)
{
    // A slot allocated in one frame and cleared in a deeper one refills more state gas than the
    // child was given, so the child returns a bigger reservoir than it received. The credit must
    // reach the `gas_left` that funded the spilled allocation charge.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    static constexpr auto CLEARER = 0xdead_address;
    pre[CLEARER] = {.code = sstore(1, 0)};
    pre[To] = {.code = sstore(1, 1) + delegatecall(CLEARER).gas(0xffff) + OP_STOP};

    // Pre-refund: 21000 intrinsic + 30 (ten PUSHes) + 5000 (cold slot allocation)
    // + 2600 (cold DELEGATECALL) + 100 (warm clear) = 28730. The clear refunds 2800.
    expect.gas_used = 28730 - 2800;
    expect.gas_refund = 2800;
    expect.state_gas = 0;
    expect.post[To].exists = true;
    expect.post[CLEARER].exists = true;
}

namespace
{
/// The code deposit of a maximum-size contract, split into its two components (EIP-8037).
constexpr int64_t DEPOSIT_CODE_SIZE = MAX_CODE_SIZE_AMSTERDAM;
constexpr auto DEPOSIT_CODE_WORDS = DEPOSIT_CODE_SIZE / 32;
constexpr auto DEPOSIT_REGULAR = 6 * DEPOSIT_CODE_WORDS;
constexpr auto DEPOSIT_STATE = DEPOSIT_CODE_SIZE * COST_PER_STATE_BYTE;

/// Gas cap leaving the initcode frame in the middle of the window where the deposit's state
/// component is affordable and its regular component is not: enough for CREATE and the memory the
/// returned code needs, plus half the regular component. The CREATE price is the only term a
/// reprice has moved, so it is taken from the cost table rather than pinned.
constexpr auto DEPOSIT_MEMORY =
    3 * DEPOSIT_CODE_WORDS + DEPOSIT_CODE_WORDS * DEPOSIT_CODE_WORDS / 512;
constexpr auto DEPOSIT_GAS_CAP =
    instr::gas_costs[EVMC_AMSTERDAM][OP_CREATE] + DEPOSIT_MEMORY + DEPOSIT_REGULAR / 2;
}  // namespace

TEST_F(state_transition, eip8037_code_deposit_out_of_regular_gas_with_a_full_reservoir)
{
    // The code deposit splits into a regular and a state component. A reservoir that covers the
    // state component must not let the deposit through when the regular component is unaffordable.
    static constexpr auto CREATOR = 0xbbbb_address;
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = state::MAX_TX_GAS_LIMIT + DEPOSIT_STATE + 1'000'000;
    block.gas_limit = tx.gas_limit;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + 1;
    tx.to = To;

    const auto initcode = ret(0, DEPOSIT_CODE_SIZE);
    pre[CREATOR] = {
        .code = mstore(0, push(initcode)) + create().input(32 - initcode.size(), initcode.size())};
    pre[To] = {.code = call(CREATOR).gas(DEPOSIT_GAS_CAP) + OP_STOP};

    expect.post[To].exists = true;
    expect.post[CREATOR].nonce = pre[CREATOR].nonce + 1;  // bumped by the CREATE
    expect.post[compute_create_address(CREATOR, pre[CREATOR].nonce)].exists = false;
}

TEST_F(state_transition, eip8037_code_deposit_regular_gas_boundary)
{
    // The same deposit one regular component richer succeeds, pinning the case above to the
    // regular gas rather than to anything else the CREATE pays for.
    static constexpr auto CREATOR = 0xbbbb_address;
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = state::MAX_TX_GAS_LIMIT + DEPOSIT_STATE + 1'000'000;
    block.gas_limit = tx.gas_limit;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + 1;
    tx.to = To;

    const auto initcode = ret(0, DEPOSIT_CODE_SIZE);
    pre[CREATOR] = {
        .code = mstore(0, push(initcode)) + create().input(32 - initcode.size(), initcode.size())};
    pre[To] = {.code = call(CREATOR).gas(DEPOSIT_GAS_CAP + DEPOSIT_REGULAR) + OP_STOP};

    expect.post[To].exists = true;
    expect.post[CREATOR].nonce = pre[CREATOR].nonce + 1;
    expect.post[compute_create_address(CREATOR, pre[CREATOR].nonce)].code =
        bytes(DEPOSIT_CODE_SIZE, 0x00);
}
