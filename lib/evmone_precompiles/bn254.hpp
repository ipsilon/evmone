// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ecc.hpp"
#include <optional>
#include <span>
#include <vector>

namespace evmmax::bn254
{
using namespace intx;

/// The BN254 curve parameters.
struct Curve
{
    /// The field/scalar unsigned int type.
    using uint_type = uint256;

    /// The order of the curve (N).
    static constexpr auto ORDER =
        0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001_u256;

    struct FpSpec
    {
        /// The field prime number (P).
        static constexpr auto ORDER =
            0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
    };
    using Fp = ecc::FieldElement<FpSpec>;

    static constexpr auto& FIELD_PRIME = Fp::ORDER;

    static constexpr auto A = 0;
    static constexpr auto B = Fp{3};

    /// Endomorphism parameters. See ecc::decompose().
    /// @{
    /// λ
    static constexpr auto LAMBDA = 0xb3c4d79d41a917585bfc41088d8daaa78b17ea66b99c90dd_u256;
    /// β
    static constexpr Fp BETA{0x59e26bcea0d48bacd4f263f1acdb5c4f5763473177fffffe_u256};
    /// x₁
    static constexpr auto X1 = 0x6f4d8248eeb859fd95b806bca6f338ee_u256;
    /// -y₁
    static constexpr auto MINUS_Y1 = 0x6f4d8248eeb859fbf83e9682e87cfd45_u256;
    /// x₂
    static constexpr auto X2 = 0x6f4d8248eeb859fc8211bbeb7d4f1128_u256;
    /// y₂
    static constexpr auto Y2 = 0x6f4d8248eeb859fd0be4e1541221250b_u256;
    /// @}
};

using AffinePoint = ecc::AffinePoint<Curve>;

using Point = ecc::Point<uint256>;
/// Note that real part of G2 value goes first and imaginary part is the second. i.e (a + b*i)
/// The pairing check precompile EVM ABI presumes that imaginary part goes first.
using ExtPoint = ecc::Point<std::pair<uint256, uint256>>;

/// Validates that point is from the bn254 curve group
///
/// Returns true if y^2 == x^3 + 3. Input is converted to the Montgomery form.
bool validate(const AffinePoint& pt) noexcept;

/// Scalar multiplication in bn254 curve group.
///
/// Computes [c]P for a point in affine coordinate on the bn254 curve,
AffinePoint mul(const AffinePoint& pt, const uint256& c) noexcept;

/// ate paring implementation for bn254 curve according to https://eips.ethereum.org/EIPS/eip-197
///
/// @param pairs  Sequence of point pairs: a point from the bn254 curve G1 group over the base field
///               followed by a point from twisted curve G2 group over extension field Fq^2.
/// @return       `true` when  ∏e(vG2[i], vG1[i]) == 1 for i in [0, n] else `false`.
///               std::nullopt on error.
std::optional<bool> pairing_check(std::span<const std::pair<Point, ExtPoint>> pairs) noexcept;

}  // namespace evmmax::bn254
