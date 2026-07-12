// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

namespace evmone
{
/// The limit of the size of created contract
/// defined by [EIP-170](https://eips.ethereum.org/EIPS/eip-170).
constexpr auto MAX_CODE_SIZE = 0x6000;

/// The limit of the size of init codes for contract creation
/// defined by [EIP-3860](https://eips.ethereum.org/EIPS/eip-3860).
constexpr auto MAX_INITCODE_SIZE = 2 * MAX_CODE_SIZE;

/// The increased limit of the size of created contract in Amsterdam
/// defined by [EIP-7954](https://eips.ethereum.org/EIPS/eip-7954).
constexpr auto MAX_CODE_SIZE_AMSTERDAM = 0x10000;

/// The increased limit of the size of init codes in Amsterdam (EIP-7954).
constexpr auto MAX_INITCODE_SIZE_AMSTERDAM = 2 * MAX_CODE_SIZE_AMSTERDAM;

/// The maximum allowed account's nonce value: 2⁶⁴-1.
/// Transactions and create instructions with nonce equal or above this value are invalid.
/// Defined by [EIP-2681](https://eips.ethereum.org/EIPS/eip-2681).
constexpr auto MAX_NONCE = 0xffff'ffff'ffff'ffff;

/// The gas given back to a value-transferring CALL, the Yellow Paper's G_callstipend.
constexpr auto CALL_STIPEND = 2300;

/// EIP-8037: fixed cost-per-state-byte.
constexpr int64_t COST_PER_STATE_BYTE = 1530;

/// EIP-8037: intrinsic state bytes charged for creating a new account.
constexpr int64_t STATE_BYTES_PER_NEW_ACCOUNT = 120;

/// EIP-8037: state bytes charged when a storage slot is newly allocated
/// (SSTORE 0 -> non-zero).
constexpr int64_t STATE_BYTES_PER_STORAGE_SET = 64;

/// EIP-8037: intrinsic state bytes charged per EIP-7702 authorization tuple
/// (excluding the new-account portion).
constexpr int64_t STATE_BYTES_PER_AUTH_BASE = 23;

/// EIP-8037: state-gas cost of creating a new account (CREATE/CREATE2,
/// CALL with value to nonexistent, SELFDESTRUCT new beneficiary, etc.).
constexpr int64_t NEW_ACCOUNT_STATE_GAS = STATE_BYTES_PER_NEW_ACCOUNT * COST_PER_STATE_BYTE;

/// EIP-8037: state-gas cost of an SSTORE 0→non-zero (slot allocation).
constexpr int64_t STORAGE_SET_STATE_GAS = STATE_BYTES_PER_STORAGE_SET * COST_PER_STATE_BYTE;

// EIP-8037 state-gas charging/refunds live on the StateGas type (state_gas.hpp).
}  // namespace evmone
