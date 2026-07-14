// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <evmc/utils.h>

namespace evmone
{
using evmc::address;
using evmc::bytes32;
using evmc::bytes_view;

/// Computes the address of the to-be-created contract with the CREATE scheme.
///
/// Computes the new account address for the contract creation context of the CREATE instruction
/// or a create transaction, as keccak256(rlp([sender, sender_nonce]))[12:].
/// This is defined by 𝐀𝐃𝐃𝐑 in Yellow Paper, 7. Contract Creation, (88-90), the case for ζ = ∅.
///
/// @param sender        The address of the message sender. YP: 𝑠.
/// @param sender_nonce  The sender's nonce before the increase. YP: 𝑛.
/// @return              The address computed with the CREATE scheme.
[[nodiscard]] EVMC_EXPORT address compute_create_address(
    const address& sender, uint64_t sender_nonce) noexcept;

/// Computes the address of the to-be-created contract with the CREATE2 scheme.
///
/// Computes the new account address for the contract creation context of the CREATE2 instruction,
/// as keccak256(0xff ++ sender ++ salt ++ keccak256(init_code))[12:] per EIP-1014.
///
/// @param sender        The address of the message sender.
/// @param salt          The salt.
/// @param init_code     The init_code to hash (initcode or initcontainer).
/// @return              The address computed with the CREATE2 scheme.
[[nodiscard]] EVMC_EXPORT address compute_create2_address(
    const address& sender, const bytes32& salt, bytes_view init_code) noexcept;
}  // namespace evmone
