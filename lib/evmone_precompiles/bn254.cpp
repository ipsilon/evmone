// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bn254.hpp"

namespace evmmax::bn254
{
namespace
{

struct Config
{
    // Linearly independent short vectors (𝑣₁=(𝑥₁, 𝑦₁), 𝑣₂=(x₂, 𝑦₂)) such that f(𝑣₁) = f(𝑣₂) = 0,
    // where f : ℤ×ℤ → ℤₙ is defined as (𝑖,𝑗) → (𝑖+𝑗λ), where λ² + λ ≡ -1 mod n. n is bn245 curve
    // order. Here λ = 0xb3c4d79d41a917585bfc41088d8daaa78b17ea66b99c90dd. DET is (𝑣₁, 𝑣₂) matrix
    // determinant. For more details see https://www.iacr.org/archive/crypto2001/21390189.pdf
    static constexpr auto X1 = 0x6f4d8248eeb859fd95b806bca6f338ee_u256;
    // Y1 should be negative, hence we calculate the determinant below adding operands instead of
    // subtracting.
    static constexpr auto MINUS_Y1 = 0x6f4d8248eeb859fbf83e9682e87cfd45_u256;
    static constexpr auto X2 = 0x6f4d8248eeb859fc8211bbeb7d4f1128_u256;
    static constexpr auto Y2 = 0x6f4d8248eeb859fd0be4e1541221250b_u256;
    static constexpr auto LAMBDA = 0xb3c4d79d41a917585bfc41088d8daaa78b17ea66b99c90dd_u256;

    // Sanity checks. More details in the paper.
    static_assert((uint512{LAMBDA} * LAMBDA + LAMBDA + 1) % Curve::ORDER == 0);
    static_assert((X1 + (Curve::ORDER - MINUS_Y1) * uint512{LAMBDA}) % Curve::ORDER == 0);
    static_assert((X2 + Y2 * uint512{LAMBDA}) % Curve::ORDER == 0);
};

// For bn254 curve and β ∈ 𝔽ₚ endomorphism ϕ : E₂ → E₂ defined as (𝑥,𝑦) → (β𝑥,𝑦) calculates [λ](𝑥,𝑦)
// with only one multiplication in 𝔽ₚ. BETA value in Montgomery form;
inline constexpr auto BETA = ecc::FieldElement<Curve>::wrap(
    20006444479023397533370224967097343182639219473961804911780625968796493078869_u256);
}  // namespace

static_assert(AffinePoint{} == 0, "default constructed is the point at infinity");

bool validate(const AffinePoint& pt) noexcept
{
    const auto yy = pt.y * pt.y;
    const auto xxx = pt.x * pt.x * pt.x;
    const auto on_curve = yy == xxx + Curve::B;
    return on_curve || pt == 0;
}

AffinePoint mul(const AffinePoint& pt, const uint256& c) noexcept
{
    if (pt == 0)
        return pt;

    if (c == 0)
        return {};

    const auto [k1, k2] = ecc::decompose<Config>(c);

    // Verify k ≡ k1 + λ·k2 (mod r)
#ifndef NDEBUG
    {
        constexpr ModArith r{Curve::ORDER};
        auto r_k1 = r.to_mont(k1.second);
        if (k1.first)
            r_k1 = r.sub(0, r_k1);
        auto r_k2 = r.to_mont(k2.second);
        if (k2.first)
            r_k2 = r.sub(0, r_k2);

        const auto r_k = r.to_mont(c);

        const auto right = r.add(r_k1, r.mul(r_k2, r.to_mont(Config::LAMBDA)));
        assert(r_k == right);
    }
#endif

    const auto q = AffinePoint{BETA * pt.x, !k2.first ? pt.y : -pt.y};
    const auto p = !k1.first ? pt : AffinePoint{pt.x, -pt.y};

    const auto pr = msm(k1.second, p, k2.second, q);

    return ecc::to_affine(pr);
}
}  // namespace evmmax::bn254
