// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "secp256r1.hpp"

namespace evmmax::secp256r1
{
namespace
{
bool is_on_curve(const AffinePoint& p) noexcept
{
    static constexpr AffinePoint::FE A{Curve::A};
    static constexpr AffinePoint::FE B{Curve::B};
    return p.y * p.y == p.x * p.x * p.x + A * p.x + B;
}
}  // namespace

bool verify(const ethash::hash256& h, const uint256& r, const uint256& s, const uint256& qx,
    const uint256& qy) noexcept
{
    // The implementation follows "Elliptic Curve Digital Signature Algorithm"
    // https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm#Signature_verification_algorithm
    // but EIP-7951 spec is also a good source:
    // https://eips.ethereum.org/EIPS/eip-7951#signature-verification-algorithm

    // 1. Validate r and s are within [1, n-1].
    if (r == 0 || r >= Curve::ORDER || s == 0 || s >= Curve::ORDER)
        return false;

    // Check that Q is not equal to the identity element O, and its coordinates are otherwise valid.
    if (qx >= Curve::FIELD_PRIME || qy >= Curve::FIELD_PRIME)
        return false;
    const AffinePoint Q{AffinePoint::FE{qx}, AffinePoint::FE{qy}};
    if (Q == 0)
        return false;

    // Check that Q lies on the curve.
    if (!is_on_curve(Q))
        return false;

    const ModArith n{Curve::ORDER};

    // 3. Let z be the Lₙ leftmost bits of e = HASH(m).
    static_assert(Curve::ORDER > 1_u256 << 255);
    const auto z = intx::be::load<uint256>(h.bytes);

    // 4. Calculate u₁ = zs⁻¹ mod n and u₂ = rs⁻¹ mod n.
    const auto s_inv = n.inv(n.to_mont(s));
    const auto u1 = n.from_mont(n.mul(n.to_mont(z), s_inv));
    const auto u2 = n.from_mont(n.mul(n.to_mont(r), s_inv));

    // 5. Calculate the curve point R = (x₁, y₁) = u₁×G + u₂×Q.
    // u1 and u2 are less than `Curve::ORDER`, so the multiplications will not reduce.
    const auto T1 = ecc::mul(G, u1);
    const auto T2 = ecc::mul(Q, u2);
    assert(T2 != 0);  // Because u2 != 0 and R != 0.
    const auto jR = ecc::add(T1, T2);
    const auto R = ecc::to_affine(jR);

    //    If R is at infinity, the signature is invalid.
    //    In this case x₁ is 0 and cannot be equal to r.
    // 6. The signature is valid if r ≡ x₁ (mod n).
    auto x1 = R.x.value();
    if (x1 >= Curve::ORDER)
        x1 -= Curve::ORDER;

    return x1 == r;
}
}  // namespace evmmax::secp256r1
