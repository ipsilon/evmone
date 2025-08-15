// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#include "secp256k1.hpp"
#include "keccak.hpp"

namespace evmmax::secp256k1
{
namespace
{
constexpr ModArith Fp{FieldPrime};
constexpr auto B = Fp.to_mont(7);
constexpr auto B3 = Fp.to_mont(7 * 3);

constexpr Point G{0x79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798_u256,
    0x483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8_u256};

struct Config
{
    // Linearly independent short vectors (𝑣₁=(𝑥₁, 𝑦₁), 𝑣₂=(x₂, 𝑦₂)) such that f(𝑣₁) = f(𝑣₂) = 0,
    // where f : ℤ×ℤ → ℤₙ is defined as (𝑖,𝑗) → (𝑖+𝑗λ), where λ² + λ ≡ -1 mod n. n is secp256k1
    // curve order. Here λ = 0x5363ad4cc05c30e0a5261c028812645a122e22ea20816678df02967c1b23bd72. DET
    // is (𝑣₁, 𝑣₂) matrix determinant. For more details see
    // https://www.iacr.org/archive/crypto2001/21390189.pdf
    static constexpr auto X1 = 64502973549206556628585045361533709077_u512;
    // Y1 should be negative, hence we calculate the determinant below adding operands instead of
    // subtracting.
    static constexpr auto Y1 = 303414439467246543595250775667605759171_u512;
    static constexpr auto X2 = 367917413016453100223835821029139468248_u512;
    static constexpr auto Y2 = 64502973549206556628585045361533709077_u512;
    // For secp256k1 the determinant equals curve order.
    static constexpr auto DET = uint512(Order);
};
// For secp256k1 curve and β ∈ 𝔽ₚ endomorphism ϕ : E₂ → E₂ defined as (𝑥,𝑦) → (β𝑥,𝑦) calculates
// [λ](𝑥,𝑦) with only one multiplication in 𝔽ₚ. BETA value in Montgomery form;
inline constexpr auto BETA =
    55313291615161283318657529331139468956476901535073802794763309073431015819598_u256;

}  // namespace

// FIXME: Change to "uncompress_point".
std::optional<uint256> calculate_y(
    const ModArith<uint256>& m, const uint256& x, bool y_parity) noexcept
{
    // Calculate sqrt(x^3 + 7)
    const auto x3 = m.mul(m.mul(x, x), x);
    const auto y = field_sqrt(m, m.add(x3, B));
    if (!y.has_value())
        return std::nullopt;

    // Negate if different parity requested
    const auto candidate_parity = (m.from_mont(*y) & 1) != 0;
    return (candidate_parity == y_parity) ? *y : m.sub(0, *y);
}

Point add(const Point& p, const Point& q) noexcept
{
    if (p.is_inf())
        return q;
    if (q.is_inf())
        return p;

    const auto pp = ecc::to_proj(Fp, p);
    const auto pq = ecc::to_proj(Fp, q);

    // b3 == 21 for y^2 == x^3 + 7
    const auto r = ecc::add(Fp, pp, pq, B3);
    return ecc::to_affine(Fp, r);
}

Point mul(const Point& p, const uint256& c) noexcept
{
    if (p.is_inf())
        return p;

    if (c == 0)
        return {0, 0};

    const Point p_mont{Fp.to_mont(p.x), Fp.to_mont(p.y)};
    const auto r = ecc::mul(Fp, p_mont, c, B3);
    return ecc::to_affine(Fp, r);
}

evmc::address to_address(const Point& pt) noexcept
{
    // This performs Ethereum's address hashing on an uncompressed pubkey.
    uint8_t serialized[64];
    intx::be::unsafe::store(serialized, pt.x);
    intx::be::unsafe::store(serialized + 32, pt.y);

    const auto hashed = ethash::keccak256(serialized, sizeof(serialized));
    evmc::address ret{};
    std::memcpy(ret.bytes, hashed.bytes + 12, 20);

    return ret;
}

std::optional<Point> secp256k1_ecdsa_recover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept
{
    // Follows
    // https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm#Public_key_recovery

    // 1. Validate r and s are within [1, n-1].
    if (r == 0 || r >= Order || s == 0 || s >= Order)
        return std::nullopt;

    // 3. Hash of the message is already calculated in e.
    // 4. Convert hash e to z field element by doing z = e % n.
    //    https://www.rfc-editor.org/rfc/rfc6979#section-2.3.2
    //    We can do this by n - e because n > 2^255.
    static_assert(Order > 1_u256 << 255);
    auto z = intx::be::load<uint256>(e.bytes);
    if (z >= Order)
        z -= Order;


    const ModArith<uint256> n{Order};

    // 5. Calculate u1 and u2.
    const auto r_n = n.to_mont(r);
    const auto r_inv = n.inv(r_n);

    const auto z_mont = n.to_mont(z);
    const auto z_neg = n.sub(0, z_mont);
    const auto u1_mont = n.mul(z_neg, r_inv);
    const auto u1 = n.from_mont(u1_mont);

    const auto s_mont = n.to_mont(s);
    const auto u2_mont = n.mul(s_mont, r_inv);
    const auto u2 = n.from_mont(u2_mont);

    // 2. Calculate y coordinate of R from r and v.
    const auto r_mont = Fp.to_mont(r);
    const auto y_mont = calculate_y(Fp, r_mont, v);
    if (!y_mont.has_value())
        return std::nullopt;
    const auto y = Fp.from_mont(*y_mont);

    // 6. Calculate public key point Q.
    const auto R = Point{r, y};

    const auto [u1k1, u1k2] = ecc::decompose<Config>(u1);
    const auto [u2k1, u2k2] = ecc::decompose<Config>(u2);

    const Point LG = ecc::to_mont(Fp, Point{Fp.mul(BETA, G.x), !u1k2.first ? G.y : Fp.sub(0, G.y)});
    const Point LR = ecc::to_mont(Fp, Point{Fp.mul(BETA, R.x), !u2k2.first ? R.y : Fp.sub(0, R.y)});

    const auto Q = ecc::add(Fp,
        shamir_multiply(Fp, u1k1.second,
            ecc::to_mont(Fp, !u1k1.first ? G : Point{G.x, Fp.sub(0, G.y)}), u1k2.second, LG, B3),
        shamir_multiply(Fp, u2k1.second,
            ecc::to_mont(Fp, !u2k1.first ? R : Point{R.x, Fp.sub(0, R.y)}), u2k2.second, LR, B3),
        B3);

    // Any other validity check needed?
    if (Q.is_inf())
        return std::nullopt;

    return ecc::to_affine(Fp, Q);
}

std::optional<evmc::address> ecrecover(
    const ethash::hash256& e, const uint256& r, const uint256& s, bool v) noexcept
{
    const auto point = secp256k1_ecdsa_recover(e, r, s, v);
    if (!point.has_value())
        return std::nullopt;

    return to_address(*point);
}

std::optional<uint256> field_sqrt(const ModArith<uint256>& m, const uint256& x) noexcept
{
    // Computes modular exponentiation
    // x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff0c
    // Operations: 253 squares 13 multiplies
    // Main part generated by github.com/mmcloughlin/addchain v0.4.0.
    //   addchain search 0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff0c
    //     > secp256k1_sqrt.acc
    //   addchain gen -tmpl expmod.tmpl secp256k1_sqrt.acc
    //     > secp256k1_sqrt.cpp
    //
    // Exponentiation computation is derived from the addition chain:
    //
    // _10      = 2*1
    // _11      = 1 + _10
    // _1100    = _11 << 2
    // _1111    = _11 + _1100
    // _11110   = 2*_1111
    // _11111   = 1 + _11110
    // _1111100 = _11111 << 2
    // _1111111 = _11 + _1111100
    // x11      = _1111111 << 4 + _1111
    // x22      = x11 << 11 + x11
    // x27      = x22 << 5 + _11111
    // x54      = x27 << 27 + x27
    // x108     = x54 << 54 + x54
    // x216     = x108 << 108 + x108
    // x223     = x216 << 7 + _1111111
    // return     ((x223 << 23 + x22) << 6 + _11) << 2

    // Allocate Temporaries.
    uint256 z;
    uint256 t0;
    uint256 t1;
    uint256 t2;
    uint256 t3;


    // Step 1: z = x^0x2
    z = m.mul(x, x);

    // Step 2: z = x^0x3
    z = m.mul(x, z);

    // Step 4: t0 = x^0xc
    t0 = m.mul(z, z);
    for (int i = 1; i < 2; ++i)
        t0 = m.mul(t0, t0);

    // Step 5: t0 = x^0xf
    t0 = m.mul(z, t0);

    // Step 6: t1 = x^0x1e
    t1 = m.mul(t0, t0);

    // Step 7: t2 = x^0x1f
    t2 = m.mul(x, t1);

    // Step 9: t1 = x^0x7c
    t1 = m.mul(t2, t2);
    for (int i = 1; i < 2; ++i)
        t1 = m.mul(t1, t1);

    // Step 10: t1 = x^0x7f
    t1 = m.mul(z, t1);

    // Step 14: t3 = x^0x7f0
    t3 = m.mul(t1, t1);
    for (int i = 1; i < 4; ++i)
        t3 = m.mul(t3, t3);

    // Step 15: t0 = x^0x7ff
    t0 = m.mul(t0, t3);

    // Step 26: t3 = x^0x3ff800
    t3 = m.mul(t0, t0);
    for (int i = 1; i < 11; ++i)
        t3 = m.mul(t3, t3);

    // Step 27: t0 = x^0x3fffff
    t0 = m.mul(t0, t3);

    // Step 32: t3 = x^0x7ffffe0
    t3 = m.mul(t0, t0);
    for (int i = 1; i < 5; ++i)
        t3 = m.mul(t3, t3);

    // Step 33: t2 = x^0x7ffffff
    t2 = m.mul(t2, t3);

    // Step 60: t3 = x^0x3ffffff8000000
    t3 = m.mul(t2, t2);
    for (int i = 1; i < 27; ++i)
        t3 = m.mul(t3, t3);

    // Step 61: t2 = x^0x3fffffffffffff
    t2 = m.mul(t2, t3);

    // Step 115: t3 = x^0xfffffffffffffc0000000000000
    t3 = m.mul(t2, t2);
    for (int i = 1; i < 54; ++i)
        t3 = m.mul(t3, t3);

    // Step 116: t2 = x^0xfffffffffffffffffffffffffff
    t2 = m.mul(t2, t3);

    // Step 224: t3 = x^0xfffffffffffffffffffffffffff000000000000000000000000000
    t3 = m.mul(t2, t2);
    for (int i = 1; i < 108; ++i)
        t3 = m.mul(t3, t3);

    // Step 225: t2 = x^0xffffffffffffffffffffffffffffffffffffffffffffffffffffff
    t2 = m.mul(t2, t3);

    // Step 232: t2 = x^0x7fffffffffffffffffffffffffffffffffffffffffffffffffffff80
    for (int i = 0; i < 7; ++i)
        t2 = m.mul(t2, t2);

    // Step 233: t1 = x^0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffff
    t1 = m.mul(t1, t2);

    // Step 256: t1 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffff800000
    for (int i = 0; i < 23; ++i)
        t1 = m.mul(t1, t1);

    // Step 257: t0 = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff
    t0 = m.mul(t0, t1);

    // Step 263: t0 = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc0
    for (int i = 0; i < 6; ++i)
        t0 = m.mul(t0, t0);

    // Step 264: z = x^0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc3
    z = m.mul(z, t0);

    // Step 266: z = x^0x3fffffffffffffffffffffffffffffffffffffffffffffffffffffffbfffff0c
    for (int i = 0; i < 2; ++i)
        z = m.mul(z, z);

    if (m.mul(z, z) != x)
        return std::nullopt;  // Computed value is not the square root.

    return z;
}
}  // namespace evmmax::secp256k1
