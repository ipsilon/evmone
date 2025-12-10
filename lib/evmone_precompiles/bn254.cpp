// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bn254.hpp"

namespace evmmax::bn254
{
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

    const auto [k1, k2] = ecc::decompose<Curve>(c);

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

        const auto right = r.add(r_k1, r.mul(r_k2, r.to_mont(Curve::LAMBDA)));
        assert(r_k == right);
    }
#endif

    const auto q = AffinePoint{Curve::BETA * pt.x, !k2.first ? pt.y : -pt.y};
    const auto p = !k1.first ? pt : AffinePoint{pt.x, -pt.y};

    const auto pr = msm(k1.second, p, k2.second, q);

    return ecc::to_affine(pr);
}
}  // namespace evmmax::bn254
