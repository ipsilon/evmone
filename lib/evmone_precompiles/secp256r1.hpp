// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ecc.hpp"
#include "hash_types.h"

namespace evmmax::secp256r1
{
using namespace intx;

struct Curve
{
    using uint_type = uint256;

    /// The field prime number (P).
    static constexpr auto FIELD_PRIME =
        0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff_u256;

    /// The secp256r1 curve group order (N).
    static constexpr auto ORDER =
        0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551_u256;

    struct FpSpec {
        static constexpr auto PRIME = FIELD_PRIME;
    };

    using Fp = ecc::FieldElement<FpSpec>;

    static constexpr auto A = FIELD_PRIME - 3;

    static constexpr auto B =
        0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b_u256;
};

using AffinePoint = ecc::AffinePoint<Curve>;

constexpr AffinePoint G{0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296_u256,
    0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5_u256};

bool verify(const ethash::hash256& h, const uint256& r, const uint256& s, const uint256& qx,
    const uint256& qy) noexcept;

}  // namespace evmmax::secp256r1
