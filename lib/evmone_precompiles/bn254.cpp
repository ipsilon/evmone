// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bn254.hpp"

namespace evmmax::bn254
{
namespace
{
constexpr ModArith Fp{FieldPrime};
constexpr auto B = Fp.to_mont(3);
constexpr auto B3 = Fp.to_mont(3 * 3);

struct Config
{
    // Linearly independent short vectors (𝑣₁=(𝑥₁, 𝑦₁), 𝑣₂=(x₂, 𝑦₂)) such that f(𝑣₁) = f(𝑣₂) = 0,
    // where f : ℤ×ℤ → ℤₙ is defined as (𝑖,𝑗) → (𝑖+𝑗λ), where λ² + λ ≡ -1 mod n. n is bn245 curve
    // order. Here λ = 0xb3c4d79d41a917585bfc41088d8daaa78b17ea66b99c90dd. DET is (𝑣₁, 𝑣₂) matrix
    // determinant. For more details see https://www.iacr.org/archive/crypto2001/21390189.pdf
    static constexpr auto X1 = 147946756881789319020627676272574806254_u512;
    // Y1 should be negative, hence we calculate the determinant below adding operands instead of
    // subtracting.
    static constexpr auto Y1 = 147946756881789318990833708069417712965_u512;
    static constexpr auto X2 = 147946756881789319000765030803803410728_u512;
    static constexpr auto Y2 = 147946756881789319010696353538189108491_u512;
    static constexpr auto DET =
        43776485743678550444492811490514550177096728800832068687396408373151616991234_u256;
};

// For bn254 curve and β ∈ 𝔽ₚ endomorphism ϕ : E₂ → E₂ defined as (𝑥,𝑦) → (β𝑥,𝑦) calculates [λ](𝑥,𝑦)
// with only one multiplication in 𝔽ₚ. BETA value in Montgomery form;
inline constexpr auto BETA =
    20006444479023397533370224967097343182639219473961804911780625968796493078869_u256;

}  // namespace

bool validate(const Point& pt) noexcept
{
    if (pt.is_inf())
        return true;

    const auto xm = Fp.to_mont(pt.x);
    const auto ym = Fp.to_mont(pt.y);
    const auto y2 = Fp.mul(ym, ym);
    const auto x2 = Fp.mul(xm, xm);
    const auto x3 = Fp.mul(x2, xm);
    const auto x3_3 = Fp.add(x3, B);
    return y2 == x3_3;
}

Point add(const Point& p, const Point& q) noexcept
{
    return ecc::add(Fp, p, q);
}

Point mul(const Point& pt, const uint256& c) noexcept
{
    if (pt.is_inf())
        return pt;

    if (c == 0)
        return {};

    const auto [k1, k2] = ecc::decompose<Config>(c % Order);

    const ecc::Point<uint256> q = {Fp.mul(BETA, Fp.to_mont(pt.x)),
        !k2.first ? Fp.to_mont(pt.y) : Fp.to_mont(FieldPrime - pt.y)};

    const ecc::Point<uint256> p =
        !k1.first ? ecc::to_mont(Fp, pt) : ecc::to_mont(Fp, Point{pt.x, FieldPrime - pt.y});

    const auto pr = shamir_multiply(Fp, k1.second, p, k2.second, q, B3);

    return ecc::to_affine(Fp, pr);
}
}  // namespace evmmax::bn254
