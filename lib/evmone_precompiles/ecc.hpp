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
    consteval explicit(false) Constant(int v) noexcept
    {
        if (N != v)
            intx::unreachable();
    }
};
using zero_t = Constant<0>;
using one_t = Constant<1>;

/// A representation of an element in a prime field.
///
/// TODO: Combine with BaseFieldElem.
template <typename Curve>
struct FieldElement
{
    using uint_type = Curve::uint_type;
    static constexpr auto& Fp = Curve::Fp;

    // TODO: Make this private.
    uint_type value_{};

    FieldElement() = default;

    constexpr explicit FieldElement(uint_type v) : value_{Fp.to_mont(v)} {}

    constexpr uint_type value() const noexcept { return Fp.from_mont(value_); }

    static constexpr FieldElement from_bytes(std::span<const uint8_t, sizeof(uint_type)> b) noexcept
    {
        // TODO: Add intx::load from std::span.
        return FieldElement{intx::be::unsafe::load<uint_type>(b.data())};
    }

    constexpr void to_bytes(std::span<uint8_t, sizeof(uint_type)> b) const noexcept
    {
        // TODO: Add intx::store to std::span.
        intx::be::unsafe::store(b.data(), value());
    }


    constexpr explicit operator bool() const noexcept { return static_cast<bool>(value_); }

    friend constexpr bool operator==(const FieldElement&, const FieldElement&) = default;

    friend constexpr bool operator==(const FieldElement& a, zero_t) noexcept { return !a.value_; }

    friend constexpr auto operator*(const FieldElement& a, const FieldElement& b) noexcept
    {
        return wrap(Fp.mul(a.value_, b.value_));
    }

    friend constexpr auto operator+(const FieldElement& a, const FieldElement& b) noexcept
    {
        return wrap(Fp.add(a.value_, b.value_));
    }

    friend constexpr auto operator-(const FieldElement& a, const FieldElement& b) noexcept
    {
        return wrap(Fp.sub(a.value_, b.value_));
    }

    friend constexpr auto operator/(one_t, const FieldElement& a) noexcept
    {
        return wrap(Fp.inv(a.value_));
    }

    friend constexpr auto operator/(const FieldElement& a, const FieldElement& b) noexcept
    {
        return wrap(Fp.mul(a.value_, Fp.inv(b.value_)));
    }

    /// Wraps a raw value into the Element type assuming it is already in Montgomery form.
    /// TODO: Make this private.
    [[gnu::always_inline]] static constexpr FieldElement wrap(const uint_type& v) noexcept
    {
        FieldElement element;
        element.value_ = v;
        return element;
    }
};

/// The affine (two coordinates) point on an Elliptic Curve over a prime field.
template <typename ValueT>
struct Point
{
    ValueT x = {};
    ValueT y = {};

    friend constexpr Point operator-(const Point& p) noexcept { return {p.x, -p.y}; }
};

/// The affine (two coordinates) point on an Elliptic Curve over a prime field.
template <typename Curve>
struct AffinePoint
{
    using FE = FieldElement<Curve>;

    FE x;
    FE y;

    AffinePoint() = default;
    constexpr AffinePoint(const FE& x_, const FE& y_) noexcept : x{x_}, y{y_} {}

    /// Create the point from literal values.
    consteval AffinePoint(const Curve::uint_type& x_value, const Curve::uint_type& y_value) noexcept
      : x{x_value}, y{y_value}
    {}

    friend constexpr bool operator==(const AffinePoint&, const AffinePoint&) = default;

    friend constexpr bool operator==(const AffinePoint& p, zero_t) noexcept
    {
        return p == AffinePoint{};
    }

    static constexpr AffinePoint from_bytes(std::span<const uint8_t, sizeof(FE) * 2> b) noexcept
    {
        const auto x = FE::from_bytes(b.template subspan<0, sizeof(FE)>());
        const auto y = FE::from_bytes(b.template subspan<sizeof(FE), sizeof(FE)>());
        return {x, y};
    }

    constexpr void to_bytes(std::span<uint8_t, sizeof(FE) * 2> b) const noexcept
    {
        x.to_bytes(b.template subspan<0, sizeof(FE)>());
        y.to_bytes(b.template subspan<sizeof(FE), sizeof(FE)>());
    }
};

template <typename Curve>
struct ProjPoint
{
    using FE = FieldElement<Curve>;
    FE x;
    FE y{1};
    FE z;
};

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

/// Converts a projected point to an affine point.
template <typename Curve>
inline AffinePoint<Curve> to_affine(const ProjPoint<Curve>& p) noexcept
{
    // This works correctly for the point at infinity (z == 0) because then z_inv == 0.
    const auto z_inv = 1 / p.z;
    return {p.x * z_inv, p.y * z_inv};
}

/// Elliptic curve point addition in affine coordinates.
///
/// Computes P ⊕ Q for two points in affine coordinates on the elliptic curve.
template <typename Curve>
AffinePoint<Curve> add(const AffinePoint<Curve>& p, const AffinePoint<Curve>& q) noexcept
{
    if (p == 0)
        return q;
    if (q == 0)
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

template <typename Curve>
ProjPoint<Curve> add(
    const ProjPoint<Curve>& p, const ProjPoint<Curve>& q, const FieldElement<Curve>& b3) noexcept
{
    static_assert(Curve::A == 0, "point addition procedure is simplified for a = 0");
    using FE = FieldElement<Curve>;

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
    FE x3;
    FE y3;
    FE z3;
    FE t0;
    FE t1;
    FE t2;
    FE t3;
    FE t4;

    t0 = x1 * x2;  // 1
    t1 = y1 * y2;  // 2
    t2 = z1 * z2;  // 3
    t3 = x1 + y1;  // 4
    t4 = x2 + y2;  // 5
    t3 = t3 * t4;  // 6
    t4 = t0 + t1;  // 7
    t3 = t3 - t4;  // 8
    t4 = y1 + z1;  // 9
    x3 = y2 + z2;  // 10
    t4 = t4 * x3;  // 11
    x3 = t1 + t2;  // 12
    t4 = t4 - x3;  // 13
    x3 = x1 + z1;  // 14
    y3 = x2 + z2;  // 15
    x3 = x3 * y3;  // 16
    y3 = t0 + t2;  // 17
    y3 = x3 - y3;  // 18
    x3 = t0 + t0;  // 19
    t0 = x3 + t0;  // 20
    t2 = b3 * t2;  // 21
    z3 = t1 + t2;  // 22
    t1 = t1 - t2;  // 23
    y3 = b3 * y3;  // 24
    x3 = t4 * y3;  // 25
    t2 = t3 * t1;  // 26
    x3 = t2 - x3;  // 27
    y3 = y3 * t0;  // 28
    t1 = t1 * z3;  // 29
    y3 = t1 + y3;  // 30
    t0 = t0 * t3;  // 31
    z3 = z3 * t4;  // 32
    z3 = z3 + t0;  // 33

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> add(
    const ProjPoint<Curve>& p, const AffinePoint<Curve>& q, const FieldElement<Curve>& b3) noexcept
{
    static_assert(Curve::A == 0, "point addition procedure is simplified for a = 0");
    using FE = FieldElement<Curve>;

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
    FE x3;
    FE y3;
    FE z3;
    FE t0;
    FE t1;
    FE t2;
    FE t3;
    FE t4;

    t0 = x1 * x2;
    t1 = y1 * y2;
    t3 = x2 + y2;
    t4 = x1 + y1;
    t3 = t3 * t4;
    t4 = t0 + t1;
    t3 = t3 - t4;
    t4 = y2 * z1;
    t4 = t4 + y1;
    y3 = x2 * z1;
    y3 = y3 + x1;
    x3 = t0 + t0;
    t0 = x3 + t0;
    t2 = b3 * z1;
    z3 = t1 + t2;
    t1 = t1 - t2;
    y3 = b3 * y3;
    x3 = t4 * y3;
    t2 = t3 * t1;
    x3 = t2 - x3;
    y3 = y3 * t0;
    t1 = t1 * z3;
    y3 = t1 + y3;
    t0 = t0 * t3;
    z3 = z3 * t4;
    z3 = z3 + t0;

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> dbl(const ProjPoint<Curve>& p, const FieldElement<Curve>& b3) noexcept
{
    static_assert(Curve::A == 0, "point doubling procedure is simplified for a = 0");
    using FE = FieldElement<Curve>;

    // Joost Renes and Craig Costello and Lejla Batina
    // "Complete addition formulas for prime order elliptic curves"
    // Cryptology ePrint Archive, Paper 2015/1060
    // https://eprint.iacr.org/2015/1060
    // Algorithm 9.

    const auto& x = p.x;
    const auto& y = p.y;
    const auto& z = p.z;
    FE x3;
    FE y3;
    FE z3;
    FE t0;
    FE t1;
    FE t2;

    t0 = y * y;    // 1
    z3 = t0 + t0;  // 2
    z3 = z3 + z3;  // 3
    z3 = z3 + z3;  // 4
    t1 = y * z;    // 5
    t2 = z * z;    // 6
    t2 = b3 * t2;  // 7
    x3 = t2 * z3;  // 8
    y3 = t0 + t2;  // 9
    z3 = t1 * z3;  // 10
    t1 = t2 + t2;  // 11
    t2 = t1 + t2;  // 12
    t0 = t0 - t2;  // 13
    y3 = t0 * y3;  // 14
    y3 = x3 + y3;  // 15
    t1 = x * y;    // 16
    x3 = t0 * t1;  // 17
    x3 = x3 + x3;  // 18

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> mul(const AffinePoint<Curve>& p, typename Curve::uint_type c,
    const FieldElement<Curve>& b3) noexcept
{
    using IntT = Curve::uint_type;

    // Reduce the scalar by the curve group order.
    // This allows using more efficient add algorithm in the loop because doubling cannot happen.
    while (true)
    {
        const auto [reduced_c, less_than] = subc(c, Curve::ORDER);
        if (less_than) [[likely]]
            break;
        c = reduced_c;
    }

    ProjPoint<Curve> r;
    const auto bit_width = sizeof(IntT) * 8 - intx::clz(c);
    for (auto i = bit_width; i != 0; --i)
    {
        r = ecc::dbl(r, b3);
        if ((c & (IntT{1} << (i - 1))) != 0)  // if the i-th bit in the scalar is set
            r = ecc::add(r, p, b3);
    }
    return r;
}
}  // namespace evmmax::ecc
