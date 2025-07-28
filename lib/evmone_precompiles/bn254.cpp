// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bn254.hpp"

namespace evmmax::bn254
{
static_assert(AffinePoint{} == 0, "default constructed is the point at infinity");

namespace
{
constexpr auto& Fp = Curve::Fp;
constexpr auto B3 = Fp.to_mont(3 * 3);
}  // namespace


bool validate(const AffinePoint& pt) noexcept
{
    static constexpr auto _3 = AffinePoint::E{3};

    // TODO: Reverse order check.
    if (pt == 0)
        return true;

    const auto yy = pt.y * pt.y;
    const auto xxx = pt.x * pt.x * pt.x;
    return yy == xxx + _3;
}

Point mul(const Point& pt, const uint256& c) noexcept
{
    if (pt.is_inf())
        return pt;

    if (c == 0)
        return {};

    const auto pr = ecc::mul(Fp, pt, c, B3);
    return ecc::to_affine(Fp, pr);
}
}  // namespace evmmax::bn254
