// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

namespace evmone
{
/// The limit of the size of created contract
/// defined by [EIP-170](https://eips.ethereum.org/EIPS/eip-170)
constexpr auto MAX_CODE_SIZE = 0x6000;

/// The limit of the size of init codes for contract creation
/// defined by [EIP-3860](https://eips.ethereum.org/EIPS/eip-3860)
constexpr auto MAX_INITCODE_SIZE = 2 * MAX_CODE_SIZE;

/// EIP-8037: fixed cost-per-state-byte (bal-devnet-7).
constexpr int64_t COST_PER_STATE_BYTE = 1530;

/// EIP-8037: intrinsic state bytes charged for creating a new account.
constexpr int64_t STATE_BYTES_PER_NEW_ACCOUNT = 120;

/// EIP-8037: intrinsic state bytes charged per non-zero storage write.
constexpr int64_t STATE_BYTES_PER_STORAGE_SET = 64;

/// EIP-8037: intrinsic state bytes charged per EIP-7702 authorization tuple
/// (excluding the new-account portion).
constexpr int64_t STATE_BYTES_PER_AUTH_BASE = 23;

/// EIP-8037: state-gas cost of creating a new account (CREATE/CREATE2,
/// CALL with value to nonexistent, SELFDESTRUCT new beneficiary, etc.).
constexpr int64_t NEW_ACCOUNT_STATE_GAS = STATE_BYTES_PER_NEW_ACCOUNT * COST_PER_STATE_BYTE;

/// EIP-8037: state-gas cost of an SSTORE 0→non-zero (slot allocation).
constexpr int64_t STORAGE_SET_STATE_GAS = STATE_BYTES_PER_STORAGE_SET * COST_PER_STATE_BYTE;

/// EIP-8037: charge `cost` from the state-gas reservoir first, spilling
/// into `gas_left` for any remainder. Atomic: returns false without
/// mutating any field when neither pool can cover the cost.
inline bool charge_state_gas(
    int64_t& gas_left, int64_t& state_gas_left, int64_t& state_gas_used, int64_t cost) noexcept
{
    if (cost <= 0)
        return true;
    if (state_gas_left >= cost)
    {
        state_gas_left -= cost;
        state_gas_used += cost;
        return true;
    }
    const auto remainder = cost - state_gas_left;
    if (gas_left >= remainder)
    {
        state_gas_used += cost;
        state_gas_left = 0;
        gas_left -= remainder;
        return true;
    }
    return false;
}
}  // namespace evmone
