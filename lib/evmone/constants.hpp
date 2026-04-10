// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace evmone
{
/// The limit of the size of created contract
/// defined by [EIP-170](https://eips.ethereum.org/EIPS/eip-170)
constexpr auto MAX_CODE_SIZE = 0x6000;

/// The limit of the size of init codes for contract creation
/// defined by [EIP-3860](https://eips.ethereum.org/EIPS/eip-3860)
constexpr auto MAX_INITCODE_SIZE = 2 * MAX_CODE_SIZE;

/// EIP-7954: increased maximum contract code size for Amsterdam.
constexpr auto MAX_CODE_SIZE_AMSTERDAM = 0x8000;

/// EIP-7954: increased maximum init code size for Amsterdam.
constexpr auto MAX_INITCODE_SIZE_AMSTERDAM = 2 * MAX_CODE_SIZE_AMSTERDAM;
}  // namespace evmone
