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

    friend constexpr bool operator==(const ProjPoint& a, const ProjPoint& b) noexcept
    {
        const auto bz2 = b.z * b.z;
        const auto az2 = a.z * a.z;

        const auto bz3 = bz2 * b.z;
        const auto az3 = az2 * a.z;

        return a.x * bz2 == b.x * az2 && a.y * bz3 == b.y * az3;
    }

    friend constexpr ProjPoint operator-(const ProjPoint& p) noexcept
    {
        return {p.x, FE{} - p.y, p.z};
    }

    static constexpr ProjPoint from(const AffinePoint<Curve>& a) noexcept
    {
        return {a.x, a.y, FE{1}};
    }
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
    const auto zz_inv = z_inv * z_inv;
    const auto zzz_inv = zz_inv * z_inv;
    return {p.x * zz_inv, p.y * zzz_inv};
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
    (void)b3;
    static_assert(Curve::A == 0, "point addition procedure is simplified for a = 0");

    if (p.z == 0)
        return q;
    assert(q.z != 0);
    assert(p != q);
    // assert(p != -q);

    // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-add-1998-cmo-2

    // Z1Z1 = Z1^2
    // Z2Z2 = Z2^2
    // U1 = X1*Z2Z2
    // U2 = X2*Z1Z1
    // t0 = Z2*Z2Z2
    // S1 = Y1*t0
    // t1 = Z1*Z1Z1
    // S2 = Y2*t1
    // H = U2-U1
    // HH = H^2
    // HHH = H*HH
    // r = S2-S1
    // V = U1*HH
    // t2 = r^2
    // t3 = 2*V
    // t4 = t2-HHH
    // X3 = t4-t3
    // t5 = V-X3
    // t6 = S1*HHH
    // t7 = r*t5
    // Y3 = t7-t6
    // t8 = Z2*H
    // Z3 = Z1*t8

    const auto& [x1, y1, z1] = p;
    const auto& [x2, y2, z2] = q;

    const auto z1z1 = z1 * z1;
    const auto z2z2 = z2 * z2;
    const auto u1 = x1 * z2z2;
    const auto u2 = x2 * z1z1;
    const auto t0 = z2 * z2z2;
    const auto s1 = y1 * t0;
    const auto t1 = z1 * z1z1;
    const auto s2 = y2 * t1;
    const auto h = u2 - u1;
    const auto hh = h * h;
    const auto hhh = h * hh;
    const auto r = s2 - s1;
    const auto v = u1 * hh;
    const auto t2 = r * r;
    const auto t3 = v + v;
    const auto t4 = t2 - hhh;
    const auto x3 = t4 - t3;
    const auto t5 = v - x3;
    const auto t6 = s1 * hhh;
    const auto t7 = r * t5;
    const auto y3 = t7 - t6;
    const auto t8 = z2 * h;
    const auto z3 = z1 * t8;

    if (p == -q)
    {
        assert(z3 == 0);
    }

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> add(
    const ProjPoint<Curve>& p, const AffinePoint<Curve>& q, const FieldElement<Curve>& b3) noexcept
{
    (void)b3;
    static_assert(Curve::A == 0, "point addition procedure is simplified for a = 0");

    if (q == 0)
        return p;

    if (p.z == 0)
        return {q.x, q.y, FieldElement<Curve>{1}};

    assert(p != ProjPoint<Curve>::from(q));
    // assert(p != -ProjPoint<Curve>::from(q));

    // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-madd

    // Z1Z1 = Z1^2
    // U2 = X2*Z1Z1
    // t0 = Z1*Z1Z1
    // S2 = Y2*t0
    // H = U2-X1
    // t1 = 2*H
    // I = t1^2
    // J = H*I
    // t2 = S2-Y1
    // r = 2*t2
    // V = X1*I
    // t3 = r^2
    // t4 = 2*V
    // t5 = t3-J
    // X3 = t5-t4
    // t6 = V-X3
    // t7 = Y1*J
    // t8 = 2*t7
    // t9 = r*t6
    // Y3 = t9-t8
    // t10 = Z1*H
    // Z3 = 2*t10

    const auto& [x1, y1, z1] = p;
    const auto& [x2, y2] = q;

    const auto z1z1 = z1 * z1;
    const auto u2 = x2 * z1z1;
    const auto t0 = z1 * z1z1;
    const auto s2 = y2 * t0;
    const auto h = u2 - x1;
    const auto t1 = h + h;
    const auto i = t1 * t1;
    const auto j = h * i;
    const auto t2 = s2 - y1;
    const auto r = t2 + t2;
    const auto v = x1 * i;
    const auto t3 = r * r;
    const auto t4 = v + v;
    const auto t5 = t3 - j;
    const auto x3 = t5 - t4;
    const auto t6 = v - x3;
    const auto t7 = y1 * j;
    const auto t8 = t7 + t7;
    const auto t9 = r * t6;
    const auto y3 = t9 - t8;
    const auto t10 = z1 * h;
    const auto z3 = t10 + t10;

    if (p == -ProjPoint<Curve>::from(q))
    {
        assert(z3 == 0);
    }

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> dbl(const ProjPoint<Curve>& p, const FieldElement<Curve>& b3) noexcept
{
    (void)b3;
    static_assert(Curve::A == 0, "point doubling procedure is simplified for a = 0");

    // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#doubling-dbl-2009-l

    // A = X1^2
    // B = Y1^2
    // C = B^2
    // t0 = X1+B
    // t1 = t0^2
    // t2 = t1-A
    // t3 = t2-C
    // D = 2*t3
    // E = 3*A
    // F = E^2
    // t4 = 2*D
    // X3 = F-t4
    // t5 = D-X3
    // t6 = 8*C
    // t7 = E*t5
    // Y3 = t7-t6
    // t8 = Y1*Z1
    // Z3 = 2*t8

    const auto& [x1, y1, z1] = p;

    const auto a = x1 * x1;
    const auto b = y1 * y1;
    const auto c = b * b;
    const auto t0 = x1 + b;
    const auto t1 = t0 * t0;
    const auto t2 = t1 - a;
    const auto t3 = t2 - c;
    const auto d = t3 + t3;
    const auto e = a + a + a;
    const auto f = e * e;
    const auto t4 = d + d;
    const auto x3 = f - t4;
    const auto t5 = d - x3;
    const auto t6 = c + c + c + c + c + c + c + c;
    const auto t7 = e * t5;
    const auto y3 = t7 - t6;
    const auto t8 = y1 * z1;
    const auto z3 = t8 + t8;

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
