// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

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
}  // namespace evmone
