// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmmax/evmmax.hpp>

#include <span>

namespace evmmax::ecc
{
template <int N>
struct Constant : std::integral_constant<int, N>
{
    consteval explicit(false) Constant(int value) noexcept
    {
        if (N != value)
            intx::unreachable();
    }
};
using zero_t = Constant<0>;
using one_t = Constant<1>;

template <typename Curve>
struct FE
{
    using uint_type = typename Curve::uint_type;
    static constexpr auto& M = Curve::M;

    uint_type value_;

    FE() = default;

    constexpr explicit FE(uint_type v) : value_(Curve::M.to_mont(v)) {}

    constexpr uint_type value() const noexcept { return Curve::M.from_mont(value_); }

    static constexpr FE from_bytes(std::span<uint8_t, sizeof(uint_type)> b) noexcept
    {
        // TODO: Add intx::load from std::span.
        return FE{intx::be::unsafe::load<uint_type>(b.data())};
    }


    constexpr explicit operator bool() const noexcept { return static_cast<bool>(value_); }

    friend constexpr bool operator==(const FE&, const FE&) = default;

    friend constexpr bool operator==(const FE& a, zero_t) noexcept { return !a.value_; }

    friend constexpr auto operator*(const FE& a, const FE& b) noexcept
    {
        return wrap(M.mul(a.value_, b.value_));
    }

    friend constexpr auto operator+(const FE& a, const FE& b) noexcept
    {
        return wrap(M.add(a.value_, b.value_));
    }

    friend constexpr auto operator-(const FE& a, const FE& b) noexcept
    {
        return wrap(M.sub(a.value_, b.value_));
    }

    friend constexpr auto operator/(const FE& a, const FE& b) noexcept
    {
        return wrap(M.mul(a.value_, M.inv(b.value_)));
    }

private:
    [[gnu::always_inline]] static constexpr FE wrap(const uint_type& v) noexcept
    {
        FE element;
        element.value_ = v;
        return element;
    }
};


template <typename Curve>
struct AffinePoint
{
    using E = FE<Curve>;

    E x;
    E y;

    friend constexpr bool operator==(const AffinePoint&, const AffinePoint&) = default;

    constexpr bool is_neutral() const noexcept { return *this == AffinePoint{}; }

    static constexpr AffinePoint from_bytes(std::span<uint8_t, sizeof(E) * 2> b)
    {
        const auto x = E::from_bytes(b.template subspan<0, sizeof(E)>());
        const auto y = E::from_bytes(b.template subspan<sizeof(E), sizeof(E)>());
        return AffinePoint{x, y};
    }
};

/// The affine (two coordinates) point on an Elliptic Curve over a prime field.
template <typename ValueT>
struct Point
{
    ValueT x = {};
    ValueT y = {};

    friend constexpr bool operator==(const Point& a, const Point& b) noexcept = default;

    friend constexpr Point operator-(const Point& p) noexcept { return {p.x, -p.y}; }

    /// Checks if the point represents the special "infinity" value.
    [[nodiscard]] constexpr bool is_inf() const noexcept { return *this == Point{}; }
};

static_assert(Point<unsigned>{}.is_inf());

template <typename IntT>
struct ProjPoint
{
    IntT x = 0;
    IntT y = 1;
    IntT z = 0;

    /// Checks if the point represents the special "infinity" value.
    [[nodiscard]] constexpr bool is_inf() const noexcept { return x == 0 && z == 0; }

    friend constexpr ProjPoint operator-(const ProjPoint& p) noexcept { return {p.x, -p.y, p.z}; }
};

static_assert(ProjPoint<unsigned>{}.is_inf());

// Jacobian (three) coordinates point implementation.
template <typename ValueT>
struct JacPoint
{
    ValueT x = 1;
    ValueT y = 1;
    ValueT z = 0;

    // Compares two Jacobian coordinates points
    friend constexpr bool operator==(const JacPoint& a, const JacPoint& b) noexcept
    {
        const auto bz2 = b.z * b.z;
        const auto az2 = a.z * a.z;

        const auto bz3 = bz2 * b.z;
        const auto az3 = az2 * a.z;

        return a.x * bz2 == b.x * az2 && a.y * bz3 == b.y * az3;
    }

    friend constexpr JacPoint operator-(const JacPoint& p) noexcept { return {p.x, -p.y, p.z}; }

    // Creates Jacobian coordinates point from affine point
    static constexpr JacPoint from(const ecc::Point<ValueT>& ap) noexcept
    {
        return {ap.x, ap.y, ValueT::one()};
    }
};

template <typename IntT>
using InvFn = IntT (*)(const ModArith<IntT>&, const IntT& x) noexcept;

/// Converts an affine point to a projected point with coordinates in Montgomery form.
template <typename IntT>
inline ProjPoint<IntT> to_proj(const ModArith<IntT>& s, const Point<IntT>& p) noexcept
{
    // FIXME: Add to_mont(1) to ModArith?
    // FIXME: Handle inf
    return {s.to_mont(p.x), s.to_mont(p.y), s.to_mont(1)};
}

/// Converts a projected point to an affine point.
template <typename IntT>
inline Point<IntT> to_affine(const ModArith<IntT>& s, const ProjPoint<IntT>& p) noexcept
{
    // FIXME: Split to_affine() and to/from_mont(). This is not good idea.
    // FIXME: Add tests for inf.
    const auto z_inv = s.inv(p.z);
    return {s.from_mont(s.mul(p.x, z_inv)), s.from_mont(s.mul(p.y, z_inv))};
}

/// Adds two elliptic curve points in affine coordinates
/// and returns the result in affine coordinates.
template <typename IntT>
Point<IntT> add(const ModArith<IntT>& m, const Point<IntT>& p, const Point<IntT>& q) noexcept
{
    if (p.is_inf())
        return q;
    if (q.is_inf())
        return p;

    const auto x1 = m.to_mont(p.x);
    const auto y1 = m.to_mont(p.y);
    const auto x2 = m.to_mont(q.x);
    const auto y2 = m.to_mont(q.y);

    // Use classic formula for point addition.
    // https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication#Point_operations

    auto dx = m.sub(x2, x1);
    auto dy = m.sub(y2, y1);
    if (dx == 0)
    {
        if (dy != 0)    // For opposite points
            return {};  // return the point at infinity.

        // For coincident points find the slope of the tangent line.
        const auto xx = m.mul(x1, x1);
        dy = m.add(m.add(xx, xx), xx);
        dx = m.add(y1, y1);
    }
    const auto slope = m.mul(dy, m.inv(dx));

    const auto xr = m.sub(m.sub(m.mul(slope, slope), x1), x2);
    const auto yr = m.sub(m.mul(m.sub(x1, xr), slope), y1);
    return {m.from_mont(xr), m.from_mont(yr)};
}

template <typename Curve>
AffinePoint<Curve> add(const AffinePoint<Curve>& p, const AffinePoint<Curve>& q) noexcept
{
    using PT = AffinePoint<Curve>;

    if (p == PT{})
        return q;
    if (q == PT{})
        return p;

    const auto& [x1, y1] = p;
    const auto& [x2, y2] = q;

    // Use classic formula for point addition.
    // https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication#Point_operations

    auto dx = x2 - x1;
    auto dy = y2 - y1;
    if (dx == 0)
    {
        if (dy != 0)    // For opposite points
            return {};  // return the point at infinity.

        // For coincident points find the slope of the tangent line.
        const auto xx = x1 * x1;
        dy = xx + xx + xx;
        dx = y1 + y1;
    }
    const auto slope = dy / dx;

    const auto xr = slope * slope - x1 - x2;
    const auto yr = slope * (x1 - xr) - y1;
    return {xr, yr};
}

template <typename IntT, int A = 0>
ProjPoint<IntT> add(const evmmax::ModArith<IntT>& s, const ProjPoint<IntT>& p,
    const ProjPoint<IntT>& q, const IntT& b3) noexcept
{
    static_assert(A == 0, "point addition procedure is simplified for a = 0");

    // Joost Renes and Craig Costello and Lejla Batina
    // "Complete addition formulas for prime order elliptic curves"
    // Cryptology ePrint Archive, Paper 2015/1060
    // https://eprint.iacr.org/2015/1060
    // Algorithm 7.

    const auto& x1 = p.x;
    const auto& y1 = p.y;
    const auto& z1 = p.z;
    const auto& x2 = q.x;
    const auto& y2 = q.y;
    const auto& z2 = q.z;
    IntT x3;
    IntT y3;
    IntT z3;
    IntT t0;
    IntT t1;
    IntT t2;
    IntT t3;
    IntT t4;

    t0 = s.mul(x1, x2);  // 1
    t1 = s.mul(y1, y2);  // 2
    t2 = s.mul(z1, z2);  // 3
    t3 = s.add(x1, y1);  // 4
    t4 = s.add(x2, y2);  // 5
    t3 = s.mul(t3, t4);  // 6
    t4 = s.add(t0, t1);  // 7
    t3 = s.sub(t3, t4);  // 8
    t4 = s.add(y1, z1);  // 9
    x3 = s.add(y2, z2);  // 10
    t4 = s.mul(t4, x3);  // 11
    x3 = s.add(t1, t2);  // 12
    t4 = s.sub(t4, x3);  // 13
    x3 = s.add(x1, z1);  // 14
    y3 = s.add(x2, z2);  // 15
    x3 = s.mul(x3, y3);  // 16
    y3 = s.add(t0, t2);  // 17
    y3 = s.sub(x3, y3);  // 18
    x3 = s.add(t0, t0);  // 19
    t0 = s.add(x3, t0);  // 20
    t2 = s.mul(b3, t2);  // 21
    z3 = s.add(t1, t2);  // 22
    t1 = s.sub(t1, t2);  // 23
    y3 = s.mul(b3, y3);  // 24
    x3 = s.mul(t4, y3);  // 25
    t2 = s.mul(t3, t1);  // 26
    x3 = s.sub(t2, x3);  // 27
    y3 = s.mul(y3, t0);  // 28
    t1 = s.mul(t1, z3);  // 29
    y3 = s.add(t1, y3);  // 30
    t0 = s.mul(t0, t3);  // 31
    z3 = s.mul(z3, t4);  // 32
    z3 = s.add(z3, t0);  // 33

    return {x3, y3, z3};
}

template <typename IntT, int A = 0>
ProjPoint<IntT> add(const ModArith<IntT>& s, const ProjPoint<IntT>& p, const Point<IntT>& q,
    const IntT& b3) noexcept
{
    (void)s;
    static_assert(A == 0, "point addition procedure is simplified for a = 0");

    // Joost Renes and Craig Costello and Lejla Batina
    // "Complete addition formulas for prime order elliptic curves"
    // Cryptology ePrint Archive, Paper 2015/1060
    // https://eprint.iacr.org/2015/1060
    // Algorithm 8.

    const auto& x1 = p.x;
    const auto& y1 = p.y;
    const auto& z1 = p.z;
    const auto& x2 = q.x;
    const auto& y2 = q.y;
    IntT x3;
    IntT y3;
    IntT z3;
    IntT t0;
    IntT t1;
    IntT t2;
    IntT t3;
    IntT t4;

    t0 = s.mul(x1, x2);
    t1 = s.mul(y1, y2);
    t3 = s.add(x2, y2);
    t4 = s.add(x1, y1);
    t3 = s.mul(t3, t4);
    t4 = s.add(t0, t1);
    t3 = s.sub(t3, t4);
    t4 = s.mul(y2, z1);
    t4 = s.add(t4, y1);
    y3 = s.mul(x2, z1);
    y3 = s.add(y3, x1);
    x3 = s.add(t0, t0);
    t0 = s.add(x3, t0);
    t2 = s.mul(b3, z1);
    z3 = s.add(t1, t2);
    t1 = s.sub(t1, t2);
    y3 = s.mul(b3, y3);
    x3 = s.mul(t4, y3);
    t2 = s.mul(t3, t1);
    x3 = s.sub(t2, x3);
    y3 = s.mul(y3, t0);
    t1 = s.mul(t1, z3);
    y3 = s.add(t1, y3);
    t0 = s.mul(t0, t3);
    z3 = s.mul(z3, t4);
    z3 = s.add(z3, t0);

    return {x3, y3, z3};
}

template <typename IntT, int A = 0>
ProjPoint<IntT> dbl(
    const evmmax::ModArith<IntT>& s, const ProjPoint<IntT>& p, const IntT& b3) noexcept
{
    static_assert(A == 0, "point doubling procedure is simplified for a = 0");

    // Joost Renes and Craig Costello and Lejla Batina
    // "Complete addition formulas for prime order elliptic curves"
    // Cryptology ePrint Archive, Paper 2015/1060
    // https://eprint.iacr.org/2015/1060
    // Algorithm 9.

    const auto& x = p.x;
    const auto& y = p.y;
    const auto& z = p.z;
    IntT x3;
    IntT y3;
    IntT z3;
    IntT t0;
    IntT t1;
    IntT t2;

    t0 = s.mul(y, y);    // 1
    z3 = s.add(t0, t0);  // 2
    z3 = s.add(z3, z3);  // 3
    z3 = s.add(z3, z3);  // 4
    t1 = s.mul(y, z);    // 5
    t2 = s.mul(z, z);    // 6
    t2 = s.mul(b3, t2);  // 7
    x3 = s.mul(t2, z3);  // 8
    y3 = s.add(t0, t2);  // 9
    z3 = s.mul(t1, z3);  // 10
    t1 = s.add(t2, t2);  // 11
    t2 = s.add(t1, t2);  // 12
    t0 = s.sub(t0, t2);  // 13
    y3 = s.mul(t0, y3);  // 14
    y3 = s.add(x3, y3);  // 15
    t1 = s.mul(x, y);    // 16
    x3 = s.mul(t0, t1);  // 17
    x3 = s.add(x3, x3);  // 18

    return {x3, y3, z3};
}

template <typename IntT>
ProjPoint<IntT> mul(
    const ModArith<IntT>& m, const Point<IntT>& p, const IntT& c, const IntT& b3) noexcept
{
    ProjPoint<IntT> r;
    const auto bit_width = sizeof(IntT) * 8 - intx::clz(c);
    for (auto i = bit_width; i != 0; --i)
    {
        r = ecc::dbl(m, r, b3);
        if ((c & (IntT{1} << (i - 1))) != 0)  // if the i-th bit in the scalar is set
            r = ecc::add(m, r, p, b3);
    }
    return r;
}
}  // namespace evmmax::ecc
