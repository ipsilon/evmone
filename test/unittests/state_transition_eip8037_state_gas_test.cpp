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
    // A create transaction colliding with an existing account (EIP-7610) returns its state-gas
    // reservoir instead of forfeiting it, so the sender is billed at most MAX_TX_GAS_LIMIT.
    rev = EVMC_AMSTERDAM;

    constexpr int64_t TX_GAS_LIMIT = 18'000'000;
    static_assert(TX_GAS_LIMIT > state::MAX_TX_GAS_LIMIT);  // The excess forms the reservoir.

    block.gas_limit = TX_GAS_LIMIT * 2;
    tx.gas_limit = TX_GAS_LIMIT;  // tx.to stays nullopt: a create transaction.
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + tx.value + 1;

    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    pre[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};

    // The collision returns before the NEW_ACCOUNT charge, so no state-gas is charged.
    expect.status = EVMC_FAILURE;
    expect.gas_used = state::MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;
    expect.state_gas = 0;
    expect.post[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};
}

namespace
{
/// Pinned: the intrinsic plus the CALL's execution gas, the NEW_ACCOUNT state charge having been
/// refilled on the light failure.
constexpr int64_t CALL_LIGHTFAIL_EXECUTION_GAS = 30'321;
}  // namespace

TEST_F(state_transition, eip8037_call_value_lightfail_new_account_charge_refilled)
{
    // A value-CALL charges NEW_ACCOUNT for an absent target before the sender-balance check.
    // The light failure creates no account, so the charge is refilled and the net state-gas is
    // zero — matching the baseline below, which differs only in the target existing.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    constexpr auto TARGET = 0xbeef_address;  // Absent from `pre`.

    pre[To] = {.code = call(TARGET).value(1).gas(0xffff) + OP_STOP};  // To cannot pay the value.

    expect.status = EVMC_SUCCESS;  // To STOPs after the light failure.
    expect.post[To].exists = true;
    expect.post[TARGET].exists = false;
    expect.gas_used = CALL_LIGHTFAIL_EXECUTION_GAS;
    expect.state_gas = 0;
}

TEST_F(state_transition, eip8037_call_value_lightfail_existing_account_baseline)
{
    // The baseline for the case above: an existing target is never charged, so both the execution
    // gas and the state-gas must come out identical.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    constexpr auto TARGET = 0xbeef_address;

    pre[To] = {.code = call(TARGET).value(1).gas(0xffff) + OP_STOP};
    pre[TARGET] = {.nonce = 1, .code = bytecode{OP_STOP}};

    expect.status = EVMC_SUCCESS;
    expect.post[To].exists = true;
    expect.post[TARGET] = {.nonce = 1};
    expect.gas_used = CALL_LIGHTFAIL_EXECUTION_GAS;
    expect.state_gas = 0;
}

TEST_F(state_transition, eip8037_value_to_zero_balance_precompile_pays_new_account)
{
    // Funding a zero-balance precompile materializes a state account, so it pays NEW_ACCOUNT
    // (EIP-161). The reservoir is empty below the cap, so the charge spills into execution gas
    // and the precompile runs on what is left.
    rev = EVMC_AMSTERDAM;
    tx.to = 0x04_address;  // Identity, absent from `pre`.
    tx.value = 1;

    constexpr int64_t IDENTITY_BASE_COST = 15;

    expect.status = EVMC_SUCCESS;
    expect.post[*tx.to].balance = 1;
    expect.gas_used = 21'000 + IDENTITY_BASE_COST + NEW_ACCOUNT_STATE_GAS;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
}

TEST_F(state_transition, eip8037_sstore_slot_allocated_and_cleared_in_one_tx)
{
    // Allocating a slot and clearing it in the same transaction (0 -> 1 -> 0) refills the
    // allocation charge, leaving the net state-gas at zero.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    pre[To] = {.code = sstore(1, 1) + sstore(1, 0)};

    // Intrinsic, four PUSHes, the cold allocation, the warm clear, less the clear's refund.
    expect.gas_used = 21'000 + 12 + 5000 + 100 - 2800;
    expect.gas_refund = 2800;
    expect.state_gas = 0;
    expect.post[To].exists = true;
}

TEST_F(state_transition, eip8037_sstore_slot_cleared_in_a_child_frame)
{
    // A slot allocated in one frame and cleared in a deeper one refills more state-gas than the
    // child was given, so the child returns a bigger reservoir than it received. The credit must
    // reach the `gas_left` that funded the spilled allocation charge.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    constexpr auto CLEARER = 0xdead_address;
    pre[CLEARER] = {.code = sstore(1, 0)};
    pre[To] = {.code = sstore(1, 1) + delegatecall(CLEARER).gas(0xffff) + OP_STOP};

    // Intrinsic, ten PUSHes, the cold allocation, the cold DELEGATECALL, the warm clear,
    // less the clear's refund.
    expect.gas_used = 21'000 + 30 + 5000 + 2600 + 100 - 2800;
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
constexpr auto DEPOSIT_EXECUTION = 6 * DEPOSIT_CODE_WORDS;
constexpr auto DEPOSIT_STATE = DEPOSIT_CODE_SIZE * COST_PER_STATE_BYTE;

/// Gas limit whose excess over the cap covers the deposit's state component outright.
constexpr auto DEPOSIT_TX_GAS = state::MAX_TX_GAS_LIMIT + DEPOSIT_STATE + 1'000'000;

/// Cap leaving the initcode frame mid-window: enough for CREATE and the memory the returned code
/// needs, plus half the execution component. The CREATE price is the only term a reprice has
/// moved, so it comes from the cost table rather than being pinned.
constexpr auto DEPOSIT_MEMORY =
    3 * DEPOSIT_CODE_WORDS + DEPOSIT_CODE_WORDS * DEPOSIT_CODE_WORDS / 512;
constexpr auto DEPOSIT_GAS_CAP =
    instr::gas_costs[EVMC_AMSTERDAM][OP_CREATE] + DEPOSIT_MEMORY + DEPOSIT_EXECUTION / 2;

constexpr auto DEPOSIT_CREATOR = 0xbbbb_address;

/// Code deploying DEPOSIT_CODE_SIZE zero bytes through a nested CREATE.
bytecode deposit_creator_code()
{
    const auto initcode = ret(0, DEPOSIT_CODE_SIZE);
    return mstore(0, push(initcode)) + create().input(32 - initcode.size(), initcode.size());
}
}  // namespace

TEST_F(state_transition, eip8037_code_deposit_out_of_execution_gas_with_a_full_reservoir)
{
    // The code deposit splits into an execution and a state component. A reservoir covering the
    // state component must not let the deposit through when the execution component is
    // unaffordable.
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = DEPOSIT_TX_GAS;
    block.gas_limit = tx.gas_limit;
    tx.to = To;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + 1;
    pre[DEPOSIT_CREATOR] = {.code = deposit_creator_code()};
    pre[To] = {.code = call(DEPOSIT_CREATOR).gas(DEPOSIT_GAS_CAP) + OP_STOP};

    expect.state_gas = 0;  // The refused deposit charges none, and the CREATE's is refilled.
    expect.post[To].exists = true;
    expect.post[DEPOSIT_CREATOR].nonce = pre[DEPOSIT_CREATOR].nonce + 1;  // Bumped by the CREATE.
    expect.post[compute_create_address(DEPOSIT_CREATOR, pre[DEPOSIT_CREATOR].nonce)].exists = false;
}

TEST_F(state_transition, eip8037_code_deposit_execution_gas_boundary)
{
    // The same deposit one execution component richer succeeds, pinning the case above to the
    // execution gas rather than to anything else the CREATE pays for.
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = DEPOSIT_TX_GAS;
    block.gas_limit = tx.gas_limit;
    tx.to = To;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + 1;
    pre[DEPOSIT_CREATOR] = {.code = deposit_creator_code()};
    pre[To] = {.code = call(DEPOSIT_CREATOR).gas(DEPOSIT_GAS_CAP + DEPOSIT_EXECUTION) + OP_STOP};

    expect.state_gas = DEPOSIT_STATE + NEW_ACCOUNT_STATE_GAS;  // Deposit plus the new account.
    expect.post[To].exists = true;
    expect.post[DEPOSIT_CREATOR].nonce = pre[DEPOSIT_CREATOR].nonce + 1;
    expect.post[compute_create_address(DEPOSIT_CREATOR, pre[DEPOSIT_CREATOR].nonce)].code =
        bytes(DEPOSIT_CODE_SIZE, 0x00);
}
