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

    static constexpr auto LAMBDA = 0x5363ad4cc05c30e0a5261c028812645a122e22ea20816678df02967c1b23bd72_u256;
    static constexpr auto X1 = 64502973549206556628585045361533709077_u256;
    static constexpr auto MINUS_Y1 = 303414439467246543595250775667605759171_u256;
    static constexpr auto X2 = 367917413016453100223835821029139468248_u256;
    static constexpr auto Y2 = 64502973549206556628585045361533709077_u256;
    static constexpr auto BETA = ecc::FieldElement<Curve>::wrap(
        55313291615161283318657529331139468956476901535073802794763309073431015819598_u256);
};

using AffinePoint = ecc::AffinePoint<Curve>;

/// Square root for secp256k1 prime field.
///
/// Computes √x mod P by computing modular exponentiation x^((P+1)/4),
/// where P is ::FieldPrime.
///
/// @return Square root of x if it exists, std::nullopt otherwise.
std::optional<ecc::FieldElement<Curve>> field_sqrt(const ecc::FieldElement<Curve>& x) noexcept;

/// Calculate y coordinate of a point having x coordinate and y parity.
std::optional<ecc::FieldElement<Curve>> calculate_y(
    const ecc::FieldElement<Curve>& x, bool y_parity) noexcept;

/// Convert the secp256k1 point (uncompressed public key) to Ethereum address.
evmc::address to_address(const AffinePoint& pt) noexcept;

std::optional<AffinePoint> secp256k1_ecdsa_recover(std::span<const uint8_t, 32> hash,
    std::span<const uint8_t, 32> r_bytes, std::span<const uint8_t, 32> s_bytes,
    bool parity) noexcept;

std::optional<evmc::address> ecrecover(std::span<const uint8_t, 32> hash,
    std::span<const uint8_t, 32> r_bytes, std::span<const uint8_t, 32> s_bytes,
    bool parity) noexcept;

}  // namespace evmmax::secp256k1
