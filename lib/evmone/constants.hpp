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

/// The maximum allowed account's nonce value: 2⁶⁴-1.
/// Transactions and create instructions with nonce equal or above this value are invalid.
/// Defined by [EIP-2681](https://eips.ethereum.org/EIPS/eip-2681).
constexpr auto MAX_NONCE = 0xffff'ffff'ffff'ffff;
}  // namespace evmone
