// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/bytes.hpp>
#include <evmc/evmc.hpp>
#include <evmc/utils.h>

namespace evmone
{
using evmc::bytes_view;

/// Prefix of code for delegated accounts
/// defined by [EIP-7702](https://eips.ethereum.org/EIPS/eip-7702)
constexpr uint8_t DELEGATION_MAGIC_BYTES[] = {0xef, 0x01, 0x00};
constexpr bytes_view DELEGATION_MAGIC{DELEGATION_MAGIC_BYTES, std::size(DELEGATION_MAGIC_BYTES)};

/// Size of the EIP-7702 delegation designator: the magic followed by the delegate address.
constexpr size_t DELEGATION_DESIGNATOR_SIZE = std::size(DELEGATION_MAGIC) + sizeof(evmc::address);

/// Check if code is an EIP-7702 delegation designator.
/// The designator is exactly the magic followed by the delegate address.
/// Code of any other size is ordinary code, even if it starts with the magic.
constexpr bool is_code_delegated(bytes_view code) noexcept
{
    return code.size() == DELEGATION_DESIGNATOR_SIZE && code.starts_with(DELEGATION_MAGIC);
}

/// Get EIP-7702 delegate address from the code of addr, if it is delegated.
EVMC_EXPORT std::optional<evmc::address> get_delegate_address(
    const evmc::HostInterface& host, const evmc::address& addr) noexcept;
}  // namespace evmone
