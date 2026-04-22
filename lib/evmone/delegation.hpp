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

/// Total length of a valid EIP-7702 delegation designator: 3-byte magic + 20-byte address.
constexpr size_t DELEGATION_DESIGNATOR_SIZE =
    std::size(DELEGATION_MAGIC_BYTES) + sizeof(evmc::address);

/// Check if code is a valid EIP-7702 delegation designator (exact 23-byte format).
constexpr bool is_code_delegated(bytes_view code) noexcept
{
    return code.size() == DELEGATION_DESIGNATOR_SIZE && code.starts_with(DELEGATION_MAGIC);
}

/// Get EIP-7702 delegate address from the code of addr, if it is delegated.
EVMC_EXPORT std::optional<evmc::address> get_delegate_address(
    const evmc::HostInterface& host, const evmc::address& addr) noexcept;
}  // namespace evmone
