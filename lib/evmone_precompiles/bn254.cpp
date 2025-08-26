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
    static constexpr auto B3 = Curve::Fp.to_mont(3 * 3);
    const auto pr = ecc::mul(pt, c, B3);
    return ecc::to_affine<Curve>(pr);
}
}  // namespace evmmax::bn254
