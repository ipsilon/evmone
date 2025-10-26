// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ecc.hpp"
#include "hash_types.h"
#include <evmc/evmc.hpp>
#include <optional>

namespace evmmax::secp256k1
{
using namespace intx;

struct Curve
{
    using uint_type = uint256;

    /// The field prime number (P).
    static constexpr auto FIELD_PRIME =
        0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;

    /// The secp256k1 curve group order (N).
    static constexpr auto ORDER =
        0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141_u256;

    static constexpr ModArith Fp{FIELD_PRIME};

    static constexpr auto A = 0;
};

using AffinePoint = ecc::AffinePoint<Curve>;

/// Square root for secp256k1 prime field.
///
/// Computes √x mod P by computing modular exponentiation x^((P+1)/4),
/// where P is ::FieldPrime.
///
/// @return Square root of x if it exists, std::nullopt otherwise.
std::optional<uint256> field_sqrt(const ModArith<uint256>& m, const uint256& x) noexcept;

/// Calculate y coordinate of a point having x coordinate and y parity.
std::optional<uint256> calculate_y(
    const ModArith<uint256>& m, const uint256& x, bool y_parity) noexcept;

/// Convert the secp256k1 point (uncompressed public key) to Ethereum address.
evmc::address to_address(const AffinePoint& pt) noexcept;

std::optional<AffinePoint> secp256k1_ecdsa_recover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept;

std::optional<evmc::address> ecrecover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept;

}  // namespace evmmax::secp256k1
