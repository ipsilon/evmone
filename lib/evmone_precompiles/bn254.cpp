// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bn254.hpp"

namespace evmmax::bn254
{
namespace
{
constexpr auto B = Fp.to_mont(3);
constexpr auto B3 = Fp.to_mont(3 * 3);
}  // namespace


bool validate(const bn254::PT& pt) noexcept
{
    if (pt.is_neutral())
        return true;

    const auto yy = pt.y * pt.y;
    const auto xxx = pt.x * pt.x * pt.x;
    return yy == xxx + PT::FE{B};
}

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

    const Point p_mont{Fp.to_mont(pt.x), Fp.to_mont(pt.y)};
    const auto pr = ecc::mul(Fp, p_mont, c, B3);

    return ecc::to_affine(Fp, pr);
}
}  // namespace evmmax::bn254
