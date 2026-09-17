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

    // The colliding account is alive, so the preparation charge does not apply.
    expect.status = EVMC_FAILURE;
    expect.gas_used = state::MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;
    expect.state_gas = 0;
    expect.post[create_address] = {.nonce = 1, .code = bytecode{OP_STOP}};
}

TEST_F(state_transition, eip8037_create_tx_revert_refunds_spilled_new_account_charge)
{
    rev = EVMC_AMSTERDAM;
    tx.data = revert(0, 0);

    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    expect.status = EVMC_REVERT;
    // Intrinsic create and initcode costs plus two PUSH1 instructions. NEW_ACCOUNT is refunded.
    expect.gas_used = 21'000 + 32'000 + 56 + 2 + 2 * instr::gas_costs[EVMC_AMSTERDAM][OP_PUSH1];
    expect.gas_refund = 0;
    expect.state_gas = 0;
    expect.post[create_address].exists = false;
}

TEST_F(state_transition, eip8037_create_tx_halt_returns_excess_reservoir)
{
    rev = EVMC_AMSTERDAM;

    constexpr int64_t TX_GAS_LIMIT = 18'000'000;
    static_assert(TX_GAS_LIMIT > state::MAX_TX_GAS_LIMIT);

    block.gas_limit = TX_GAS_LIMIT * 2;
    tx.gas_limit = TX_GAS_LIMIT;
    tx.data = bytecode{OP_INVALID};
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + tx.value + 1;

    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    expect.status = EVMC_INVALID_INSTRUCTION;
    // The halt consumes the execution-gas dimension, but the unused reservoir is returned.
    expect.gas_used = state::MAX_TX_GAS_LIMIT;
    expect.gas_refund = 0;
    expect.state_gas = 0;
    expect.post[create_address].exists = false;
}

TEST_F(state_transition, eip8037_create_tx_charges_new_account_and_code_deposit)
{
    rev = EVMC_AMSTERDAM;
    tx.data = ret(0, 1);  // Deploy a single zero byte.

    expect.state_gas = NEW_ACCOUNT_STATE_GAS + COST_PER_STATE_BYTE;
    expect.post[compute_create_address(Sender, pre[Sender].nonce)].code = bytes{0x00};
}

TEST_F(state_transition, eip8037_create_tx_with_value_pays_new_account_once)
{
    rev = EVMC_AMSTERDAM;
    tx.value = 1;

    expect.gas_used = 53'000 + NEW_ACCOUNT_STATE_GAS;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
    expect.post[compute_create_address(Sender, pre[Sender].nonce)] = {.nonce = 1, .balance = 1};
}

TEST_F(state_transition, eip8037_create_tx_uses_reservoir_then_execution_gas)
{
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = state::MAX_TX_GAS_LIMIT + NEW_ACCOUNT_STATE_GAS / 2;
    block.gas_limit = tx.gas_limit;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + tx.value + 1;

    expect.gas_used = 53'000 + NEW_ACCOUNT_STATE_GAS;
    expect.gas_refund = 0;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
    expect.post[compute_create_address(Sender, pre[Sender].nonce)].nonce = 1;
}

TEST_F(state_transition, eip8037_create_tx_to_prefunded_account_has_no_new_account_charge)
{
    rev = EVMC_AMSTERDAM;

    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    pre[create_address].balance = 1;

    expect.gas_used = 53'000;
    expect.state_gas = 0;
    expect.post[create_address] = {.nonce = 1, .balance = 1};
}

TEST_F(state_transition, eip8037_create_tx_out_of_gas_on_new_account_charge)
{
    rev = EVMC_AMSTERDAM;
    tx.gas_limit = 60'000;  // Intrinsic gas leaves less than NEW_ACCOUNT_STATE_GAS.

    expect.status = EVMC_OUT_OF_GAS;
    expect.gas_used = tx.gas_limit;
    expect.state_gas = 0;
    expect.post[compute_create_address(Sender, pre[Sender].nonce)].exists = false;
}

TEST_F(state_transition, eip8037_nested_create_revert_refills_new_account_charge)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    pre[To] = {.code = mstore(0, push(revert(0, 0))) + create().input(27, 5) + OP_STOP};

    expect.state_gas = 0;
    expect.post[To].nonce = 1;
}

namespace
{
constexpr int64_t CALL_VALUE_COST = 9000;  // Not exported by the interpreter.

/// The intrinsic plus the CALL's execution gas: its seven arguments, the warm call, the
/// cold-account surcharge and the value transfer, less the stipend a light failure never spends.
/// The NEW_ACCOUNT state charge is refilled, so it does not appear here.
constexpr int64_t CALL_LIGHTFAIL_EXECUTION_GAS =
    21'000 + 7 * instr::gas_costs[EVMC_AMSTERDAM][OP_PUSH1] +
    instr::gas_costs[EVMC_AMSTERDAM][OP_CALL] + instr::additional_cold_account_access_cost +
    CALL_VALUE_COST - CALL_STIPEND;
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

TEST_F(state_transition, eip8037_value_to_new_account_pays_new_account)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;  // Absent from pre.
    tx.value = 1;
    tx.gas_limit = 21'000 + NEW_ACCOUNT_STATE_GAS;  // Exact successful boundary.

    expect.post[To].balance = 1;
    expect.gas_used = tx.gas_limit;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
}

TEST_F(state_transition, eip8037_value_to_existing_empty_account_pays_new_account)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    tx.value = 1;
    pre[To] = {};

    expect.post[To].balance = 1;
    expect.gas_used = 21'000 + NEW_ACCOUNT_STATE_GAS;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
}

TEST_F(state_transition, eip8037_value_to_new_account_uses_reservoir_then_execution_gas)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;  // Absent from pre.
    tx.value = 1;
    tx.gas_limit = state::MAX_TX_GAS_LIMIT + NEW_ACCOUNT_STATE_GAS / 2;
    block.gas_limit = tx.gas_limit;
    pre[Sender].balance = intx::uint256{tx.gas_limit} * tx.max_gas_price + tx.value + 1;

    expect.post[To].balance = 1;
    expect.gas_used = 21'000 + NEW_ACCOUNT_STATE_GAS;
    expect.state_gas = NEW_ACCOUNT_STATE_GAS;
}

TEST_F(state_transition, eip8037_value_to_new_account_out_of_gas)
{
    rev = EVMC_AMSTERDAM;
    tx.to = To;  // Absent from pre.
    tx.value = 1;
    tx.gas_limit = 21'000 + NEW_ACCOUNT_STATE_GAS - 1;

    expect.status = EVMC_OUT_OF_GAS;
    expect.gas_used = tx.gas_limit;
    expect.state_gas = 0;
    expect.post[To].exists = false;
}

TEST_F(state_transition, eip8037_value_to_new_precompile_failure_refunds_new_account)
{
    rev = EVMC_AMSTERDAM;
    tx.to = 0x04_address;  // Identity, absent from pre.
    tx.value = 1;
    tx.gas_limit = 21'000 + NEW_ACCOUNT_STATE_GAS + 14;  // Identity requires 15 gas.

    expect.status = EVMC_OUT_OF_GAS;
    expect.gas_used = tx.gas_limit;
    expect.state_gas = 0;
    expect.post[*tx.to].exists = false;
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

TEST_F(state_transition, eip8037_reverted_child_keeps_the_slot_allocation_charged)
{
    // A child clearing a slot its caller allocated refills more state-gas than it was given,
    // leaving its reservoir above its own budget. Reverting must restore that budget rather than
    // credit the refill, so the allocation stays charged.
    rev = EVMC_AMSTERDAM;
    tx.to = To;
    constexpr auto CLEARER = 0xdead_address;
    pre[CLEARER] = {.code = sstore(1, 0) + revert(0, 0)};
    pre[To] = {.code = sstore(1, 1) + delegatecall(CLEARER).gas(0xffff) + OP_STOP};

    // Intrinsic, twelve PUSHes, the cold allocation and its state charge, the cold DELEGATECALL,
    // the reverted warm clear. The clear's refund dies with the frame.
    expect.gas_used = 21'000 + 36 + 5000 + STORAGE_SET_STATE_GAS + 2600 + 100;
    expect.gas_refund = 0;
    expect.state_gas = STORAGE_SET_STATE_GAS;
    expect.post[To].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;  // The child's clear is rolled back.
    expect.post[CLEARER].exists = true;
}

namespace
{
/// The code deposit of a maximum-size contract, split into its two components (EIP-8037).
constexpr auto DEPOSIT_CODE_WORDS = MAX_CODE_SIZE_AMSTERDAM / 32;
constexpr auto DEPOSIT_EXECUTION = 6 * DEPOSIT_CODE_WORDS;
constexpr int64_t DEPOSIT_STATE = int64_t{MAX_CODE_SIZE_AMSTERDAM} * COST_PER_STATE_BYTE;

/// Gas limit whose excess over the cap covers the deposit's state component outright.
constexpr auto DEPOSIT_TX_GAS = state::MAX_TX_GAS_LIMIT + DEPOSIT_STATE + 1'000'000;

/// Cap leaving the initcode frame mid-window: enough for CREATE and the memory the returned code
/// needs, plus half the execution component. The CREATE price is the only term a reprice has
/// moved, so it comes from the cost table rather than being pinned.
/// DEPOSIT_MEMORY mirrors the expansion formula in check_memory(); a change there shifts the
/// window rather than failing here.
constexpr auto DEPOSIT_MEMORY =
    3 * DEPOSIT_CODE_WORDS + DEPOSIT_CODE_WORDS * DEPOSIT_CODE_WORDS / 512;
constexpr auto DEPOSIT_GAS_CAP =
    instr::gas_costs[EVMC_AMSTERDAM][OP_CREATE] + DEPOSIT_MEMORY + DEPOSIT_EXECUTION / 2;

constexpr auto DEPOSIT_CREATOR = 0xbbbb_address;

/// Code deploying MAX_CODE_SIZE_AMSTERDAM zero bytes through a nested CREATE.
bytecode deposit_creator_code()
{
    const auto initcode = ret(0, MAX_CODE_SIZE_AMSTERDAM);
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
        bytes(MAX_CODE_SIZE_AMSTERDAM, 0x00);
}
