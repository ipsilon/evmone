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

    friend constexpr auto operator-(const FieldElement& a) noexcept
    {
        return wrap(Fp.sub(0, a.value_));
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

/// Elliptic curve point in Jacobian coordinates (X, Y, Z)
/// representing the affine point (X/Z², Y/Z³).
/// TODO: Merge with JacPoint.
template <typename Curve>
struct ProjPoint
{
    using FE = FieldElement<Curve>;
    FE x;
    FE y{1};  // TODO: Make sure this is compile-time constant.
    FE z;

    ProjPoint() = default;
    constexpr ProjPoint(const FE& x_, const FE& y_, const FE& z_) noexcept : x{x_}, y{y_}, z{z_} {}
    constexpr explicit ProjPoint(const AffinePoint<Curve>& p) noexcept : x{p.x}, y{p.y}, z{FE{1}} {}

    friend constexpr bool operator==(const ProjPoint& p, zero_t) noexcept { return p.z == 0; }

    friend constexpr bool operator==(const ProjPoint& p, const ProjPoint& q) noexcept
    {
        const auto& [x1, y1, z1] = p;
        const auto& [x2, y2, z2] = q;
        const auto z1z1 = z1 * z1;
        const auto z1z1z1 = z1z1 * z1;
        const auto z2z2 = z2 * z2;
        const auto z2z2z2 = z2z2 * z2;
        return x1 * z2z2 == x2 * z1z1 && y1 * z2z2z2 == y2 * z1z1z1;
    }

    friend constexpr ProjPoint operator-(const ProjPoint& p) noexcept { return {p.x, -p.y, p.z}; }
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

/// Elliptic curve point addition in Jacobian coordinates.
///
/// Computes P ⊕ Q for two points in Jacobian coordinates on the elliptic curve.
/// This procedure handles all inputs (e.g. doubling or points at infinity).
template <typename Curve>
ProjPoint<Curve> add(const ProjPoint<Curve>& p, const ProjPoint<Curve>& q) noexcept
{
    if (p == 0)
        return q;
    if (q == 0)
        // TODO: Untested and untestable via precompile call (for secp256k1 and secp256r1).
        return p;

    if (p == q)
        return dbl(p);

    // Use the "add-1998-cmo-2" formula for curve in Jacobian coordinates.
    // The cost is 12M + 4S + 6add + 1*2.
    // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-add-1998-cmo-2
    // TODO: The newer formula "add-2007-bl" trades one multiplication for one squaring and
    //   additional additions. We don't have dedicated squaring operation yet, so it's not clear
    //   if it would be faster.

    const auto& [x1, y1, z1] = p;
    const auto& [x2, y2, z2] = q;

    const auto z1z1 = z1 * z1;
    const auto z2z2 = z2 * z2;
    const auto u1 = x1 * z2z2;
    const auto u2 = x2 * z1z1;
    const auto z1z1z1 = z1 * z1z1;
    const auto z2z2z2 = z2 * z2z2;
    const auto s1 = y1 * z2z2z2;
    const auto s2 = y2 * z1z1z1;
    const auto h = u2 - u1;
    const auto r = s2 - s1;
    assert(h != 0 || r != 0);  // We already handled p == q case above.
    const auto hh = h * h;
    const auto hhh = h * hh;
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

    return {x3, y3, z3};
}

/// Mixed addition of elliptic curve points.
///
/// Computes P ⊕ Q for a point P in Jacobian coordinates and a point Q in affine coordinates.
/// This procedure is for use in point multiplication and not all inputs are supported.
template <typename Curve>
ProjPoint<Curve> add(const ProjPoint<Curve>& p, const AffinePoint<Curve>& q) noexcept
{
    assert(p != ProjPoint(q));

    if (q == 0)
        // TODO: Untested and untestable via precompile call (for secp256k1 and secp256r1).
        return p;
    if (p == 0)
        return ProjPoint(q);

    // Use the "madd" formula for curve in Jacobian coordinates.
    // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian.html#addition-madd

    const auto& [x1, y1, z1] = p;
    const auto& [x2, y2] = q;

    const auto z1z1 = z1 * z1;
    const auto u2 = x2 * z1z1;
    const auto z1z1z1 = z1 * z1z1;
    const auto s2 = y2 * z1z1z1;
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

    return {x3, y3, z3};
}

template <typename Curve>
ProjPoint<Curve> dbl(const ProjPoint<Curve>& p) noexcept
{
    const auto& [x1, y1, z1] = p;

    if constexpr (Curve::A == 0)
    {
        // Use the "dbl-2009-l" formula for a=0 curve in Jacobian coordinates.
        // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#doubling-dbl-2009-l

        const auto xx = x1 * x1;
        const auto yy = y1 * y1;
        const auto yyyy = yy * yy;
        const auto t0 = x1 + yy;
        const auto t1 = t0 * t0;
        const auto t2 = t1 - xx;
        const auto t3 = t2 - yyyy;
        const auto d = t3 + t3;
        const auto e = xx + xx + xx;
        const auto f = e * e;
        const auto t4 = d + d;
        const auto x3 = f - t4;
        const auto t6 = d - x3;
        const auto t8 = yyyy + yyyy + yyyy + yyyy + yyyy + yyyy + yyyy + yyyy;
        const auto t9 = e * t6;
        const auto y3 = t9 - t8;
        const auto t10 = y1 * z1;
        const auto z3 = t10 + t10;
        return {x3, y3, z3};
    }
    else if constexpr (Curve::A == Curve::FIELD_PRIME - 3)
    {
        // Use the "dbl-2001-b" doubling formula.
        // https://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#doubling-dbl-2001-b

        const auto zz = z1 * z1;
        const auto yy = y1 * y1;
        const auto xyy = x1 * yy;
        const auto t0 = x1 - zz;
        const auto t1 = x1 + zz;
        const auto t2 = t0 * t1;
        const auto alpha = t2 + t2 + t2;
        const auto t3 = alpha * alpha;
        const auto t4 = xyy + xyy + xyy + xyy + xyy + xyy + xyy + xyy;
        const auto x3 = t3 - t4;
        const auto t5 = y1 + z1;
        const auto t6 = t5 * t5;
        const auto t7 = t6 - yy;
        const auto z3 = t7 - zz;
        const auto t8 = xyy + xyy + xyy + xyy;
        const auto t9 = t8 - x3;
        const auto t10 = yy * yy;
        const auto t11 = t10 + t10 + t10 + t10 + t10 + t10 + t10 + t10;
        const auto t12 = alpha * t9;
        const auto y3 = t12 - t11;
        return {x3, y3, z3};
    }
    else
    {
        // TODO(c++23): Use fake always-false condition for older compilers.
        static_assert(Curve::A == 0, "unsupported Curve::A value");
    }
}

/// Tests if a specific bit is set in an integer type.
/// Bits are indexed from the least significant. Checking beyond the bit-width is undefined.
/// TODO: Move to intx.
template <typename IntT>
bool test_bit(const IntT& v, size_t bit_index) noexcept
{
    using word_type = IntT::word_type;
    static constexpr auto WORD_BITS = sizeof(word_type) * 8;
    const auto word = v[(bit_index / WORD_BITS)];
    const auto b = bit_index % WORD_BITS;
    return (word & (uint64_t{1} << b)) != 0;
}

template <typename Curve>
ProjPoint<Curve> mul(const AffinePoint<Curve>& p, typename Curve::uint_type c) noexcept
{
    using IntT = Curve::uint_type;

    // Reduce the scalar by the curve group order.
    // This allows using more efficient add algorithm in the loop because doubling cannot happen.
    while (true)
    {
        const auto [reduced_c, less_than] = subc(c, Curve::ORDER);
        if (less_than) [[likely]]
            break;
        // TODO: Untested and untestable via precompile call (for secp256k1 and secp256r1).
        c = reduced_c;
    }

    ProjPoint<Curve> r;
    const auto bit_width = sizeof(IntT) * 8 - intx::clz(c);
    for (auto i = bit_width; i != 0; --i)
    {
        r = ecc::dbl(r);
        if (test_bit(c, i - 1))
            r = ecc::add(r, p);
    }
    return r;
}
}  // namespace evmmax::ecc
