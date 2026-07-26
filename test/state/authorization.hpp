// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <intx/intx.hpp>
#include <optional>
#include <vector>

namespace evmone::state
{
using evmc::address;
using intx::uint256;

/// The set-code transaction authorization tuple (EIP-7702).
struct Authorization
{
    uint256 chain_id;
    address addr;
    uint64_t nonce = 0;

    /// The signature's y_parity. Valid values are 0 and 1, but an out-of-range value only
    /// invalidates the authorization, not the transaction.
    uint8_t y_parity = 0;

    uint256 r;
    uint256 s;

    /// The signer address. Empty if it cannot be recovered from the signature (y_parity, r, s).
    std::optional<address> signer;
};

using AuthorizationList = std::vector<Authorization>;
}  // namespace evmone::state
