// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "secp256r1.hpp"

#define UNTESTED(condition) assert(!(condition))

namespace evmmax::secp256r1
{
namespace
{
struct Curve
{
    using uint_type = uint256;

    /// The field prime number (P).
    static constexpr auto FIELD_PRIME =
        0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff_u256;

    /// The secp256k1 curve group order (N).
    static constexpr auto ORDER =
        0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551_u256;

    static constexpr ModArith Fp{FIELD_PRIME};

    static constexpr auto A =
        0xffffffff00000001000000000000000000000000fffffffffffffffffffffffc_u256;
};

constexpr auto B = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b_u256;

using AffinePoint = ecc::AffinePoint<Curve>;

constexpr AffinePoint G{0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296_u256,
    0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5_u256};

}  // namespace

bool verify(const ethash::hash256& h, const uint256& r, const uint256& s, const uint256& qx,
    const uint256& qy) noexcept
{
    // auto Rand = ecc::mul(G, 0xe1);
    // std::cout << "R: " << hex(Rand.x.value()) << ", " << hex(Rand.y.value()) << "\n";
    // R: 6b825636d1d235cc50025d1096dd72363bc1742031c0e190466d23ba13e4eab2,
    // e9095f0f5afba0bf26ce937d1bb485c371aa41ef488404c72f03ac10308dea6f


    // 1. Validate r and s are within [1, n-1].
    [[maybe_unused]] const auto r_valid = (r > 0 && r < Curve::ORDER);
    [[maybe_unused]] const auto s_valid = (s > 0 && s < Curve::ORDER);
    // UNTESTED(r == 0);
    // UNTESTED(r == Curve::ORDER);
    // UNTESTED(r == Curve::FIELD_PRIME);
    // UNTESTED(r > Curve::ORDER);
    UNTESTED(r == ~uint256{});
    // UNTESTED(s == 0);
    // UNTESTED(s == Curve::ORDER);
    // UNTESTED(s == Curve::FIELD_PRIME);
    // UNTESTED(s > Curve::ORDER);
    UNTESTED(s == ~uint256{});
    // UNTESTED(s_valid && r == 0);
    // UNTESTED(s_valid && r == Curve::ORDER);
    // UNTESTED(s_valid && r == Curve::FIELD_PRIME);
    // UNTESTED(s_valid && r > Curve::ORDER);
    UNTESTED(s_valid && r == ~uint256{});
    // UNTESTED(r_valid && s == 0);
    // UNTESTED(r_valid && s == Curve::ORDER);
    // UNTESTED(r_valid && s == Curve::FIELD_PRIME);
    // UNTESTED(r_valid && s > Curve::ORDER);
    UNTESTED(r_valid && s == ~uint256{});
    if (r == 0 || r >= Curve::ORDER || s == 0 || s >= Curve::ORDER)
        return false;

    if (qx == 0 || qx >= Curve::FIELD_PRIME || qy == 0 || qy >= Curve::FIELD_PRIME)
        return false;

    const AffinePoint Q{AffinePoint::FE{qx}, AffinePoint::FE{qy}};

    [[maybe_unused]] static constexpr AffinePoint::FE AA{Curve::A};
    [[maybe_unused]] static constexpr AffinePoint::FE BB{B};

    if (Q.x * Q.x * Q.x + AA * Q.x + BB != Q.y * Q.y)
        return false;

    const ModArith n{Curve::ORDER};

    static_assert(Curve::ORDER > 1_u256 << 255);
    const auto z = intx::be::load<uint256>(h.bytes);
    UNTESTED(z == 0);
    UNTESTED(z == Curve::ORDER);
    UNTESTED(z == Curve::FIELD_PRIME);
    // UNTESTED(z > Curve::ORDER);
    UNTESTED(z == ~uint256{});

    const auto s_inv = n.inv(n.to_mont(s));
    const auto u1 = n.from_mont(n.mul(n.to_mont(z), s_inv));
    const auto u2 = n.from_mont(n.mul(n.to_mont(r), s_inv));

    // UNTESTED(G == Q);
    // UNTESTED(u1 == u2);
    // UNTESTED(u1 == u2 && G == Q);

    const auto T1 = ecc::mul(G, u1);
    const auto T2 = ecc::mul(Q, u2);
    // UNTESTED(T1 == T2);
    // UNTESTED(T1 == T2 && G != Q);
    const auto jR = ecc::add(T1, T2);
    const auto R = ecc::to_affine(jR);
    auto rp = R.x.value();

    UNTESTED(rp == Curve::ORDER);
    // UNTESTED(rp > Curve::ORDER);
    // UNTESTED(rp < Curve::ORDER);
    if (rp >= Curve::ORDER)
        rp -= Curve::ORDER;

    if (rp == r)
        return true;

    return false;
}
}  // namespace evmmax::secp256r1
