// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "fields.hpp"

namespace evmmax::bn254
{
consteval Fq2 make_fq2(const uint256& a, const uint256& b) noexcept
{
    return Fq2({Fq::from_int(a), Fq::from_int(b)});
}

/// Defines coefficients needed for fast Frobenius endomorphism computation.
/// For more ref see https://eprint.iacr.org/2010/354.pdf 3.2 Frobenius Operator.
/// TODO: Make it constexpr.
static inline std::array<std::array<Fq2, 5>, 3> FROBENIUS_COEFFS = {
    {
        {
            make_fq2(
                8376118865763821496583973867626364092589906065868298776909617916018768340080_u256,
                16469823323077808223889137241176536799009286646108169935659301613961712198316_u256),
            make_fq2(
                21575463638280843010398324269430826099269044274347216827212613867836435027261_u256,
                10307601595873709700152284273816112264069230130616436755625194854815875713954_u256),
            make_fq2(
                2821565182194536844548159561693502659359617185244120367078079554186484126554_u256,
                3505843767911556378687030309984248845540243509899259641013678093033130930403_u256),
            make_fq2(
                2581911344467009335267311115468803099551665605076196740867805258568234346338_u256,
                19937756971775647987995932169929341994314640652964949448313374472400716661030_u256),
            make_fq2(
                685108087231508774477564247770172212460312782337200605669322048753928464687_u256,
                8447204650696766136447902020341177575205426561248465145919723016860428151883_u256),
        },
        {
            make_fq2(
                21888242871839275220042445260109153167277707414472061641714758635765020556617_u256,
                0_u256),
            make_fq2(
                21888242871839275220042445260109153167277707414472061641714758635765020556616_u256,
                0_u256),
            make_fq2(
                21888242871839275222246405745257275088696311157297823662689037894645226208582_u256,
                0_u256),
            make_fq2(2203960485148121921418603742825762020974279258880205651966_u256, 0_u256),
            make_fq2(2203960485148121921418603742825762020974279258880205651967_u256, 0_u256),
        },
        {
            make_fq2(
                11697423496358154304825782922584725312912383441159505038794027105778954184319_u256,
                303847389135065887422783454877609941456349188919719272345083954437860409601_u256),
            make_fq2(
                3772000881919853776433695186713858239009073593817195771773381919316419345261_u256,
                2236595495967245188281701248203181795121068902605861227855261137820944008926_u256),
            make_fq2(
                19066677689644738377698246183563772429336693972053703295610958340458742082029_u256,
                18382399103927718843559375435273026243156067647398564021675359801612095278180_u256),
            make_fq2(
                5324479202449903542726783395506214481928257762400643279780343368557297135718_u256,
                16208900380737693084919495127334387981393726419856888799917914180988844123039_u256),
            make_fq2(
                8941241848238582420466759817324047081148088512956452953208002715982955420483_u256,
                10338197737521362862238855242243140895517409139741313354160881284257516364953_u256),
        },
    },
};

/// Verifies that value is in the proper prime field.
constexpr bool is_field_element(const uint256& v)
{
    return v < Curve::FIELD_PRIME;
}

/// Verifies that affine point is on the curve (not twisted)
constexpr bool is_on_curve(const ecc::Point<Fq>& p) noexcept
{
    // TODO(C++23): make static
    constexpr auto B = Fq::from_int(3);

    const auto x3 = p.x * p.x * p.x;
    const auto y2 = p.y * p.y;
    return y2 == x3 + B;
}

/// Verifies that affine point over Fq^2 field is on the twisted curve.
constexpr bool is_on_twisted_curve(const evmmax::ecc::Point<Fq2>& p)
{
    const auto x3 = p.x * p.x * p.x;
    const auto y2 = p.y * p.y;

    return y2 == x3 + Fq6Config::_3_ksi_inv;
}

/// Verifies that affine point over the base field is infinity.
constexpr bool is_infinity(const evmmax::ecc::Point<Fq>& p)
{
    return p.x.is_zero() && p.y.is_zero();
}

/// Verifies that affine point over the Fq^2 extended field is infinity.
constexpr bool g2_is_infinity(const evmmax::ecc::Point<Fq2>& p)
{
    return p.x == Fq2::zero() && p.y == Fq2::zero();
}

// Forbenius endomorphism related functions are implemented based on
// https://hackmd.io/@jpw/bn254#mathbb-G_2-membership-check-using-efficient-endomorphism
// and
// https://eprint.iacr.org/2010/354.pdf 3.2 Frobenius Operator

/// Computes Frobenius endomorphism of point `p` in Jacobian coordinates from twisted curve
/// over Fq2 extended field.
/// This specialisation computes Frobenius and Frobenius^3
/// TODO: add reference that it's exactly the same as untwist->frobenius->twist
template <int P>
constexpr ecc::JacPoint<Fq2> endomorphism(const ecc::JacPoint<Fq2>& p) noexcept
    requires(P == 1 || P == 3)
{
    return {
        p.x.conjugate() * FROBENIUS_COEFFS[P - 1][1],
        p.y.conjugate() * FROBENIUS_COEFFS[P - 1][2],
        p.z.conjugate(),
    };
}

/// Computes Frobenius endomorphism of point `p` in Jacobian coordinates from twisted curve
/// over Fq^2 extended field.
/// This specialisation computes Frobenius^2
template <int P>
constexpr ecc::JacPoint<Fq2> endomorphism(const ecc::JacPoint<Fq2>& p) noexcept
    requires(P == 2)
{
    return {
        p.x * FROBENIUS_COEFFS[1][1],
        p.y * FROBENIUS_COEFFS[1][2],
        p.z,
    };
}

/// Computes Frobenius endomorphism of point `p` in affine coordinates from twisted curve
/// over Fq^2 extended field.
/// This specialisation computes Frobenius and Frobenius^3
template <int P>
constexpr ecc::Point<Fq2> endomorphism(const ecc::Point<Fq2>& p) noexcept
    requires(P == 1 || P == 3)
{
    return {
        p.x.conjugate() * FROBENIUS_COEFFS[P - 1][1],
        p.y.conjugate() * FROBENIUS_COEFFS[P - 1][2],
    };
}

/// Computes Frobenius endomorphism of point `p` in affine coordinates from twisted curve
/// over Fq^2 extended field.
/// This specialisation computes Frobenius^2
template <int P>
constexpr ecc::Point<Fq2> endomorphism(const ecc::Point<Fq2>& p) noexcept
    requires(P == 2)
{
    return {
        p.x * FROBENIUS_COEFFS[1][1],
        p.y * FROBENIUS_COEFFS[1][2],
    };
}

/// Computes Frobenius endomorphism for Fq12 field member values
/// This specialisation computes Frobenius and Frobenius^3
template <int P>
constexpr Fq12 endomorphism(const Fq12& f) noexcept
    requires(P == 1 || P == 3)
{
    return Fq12({
        Fq6({
            f.coeffs[0].coeffs[0].conjugate(),
            f.coeffs[0].coeffs[1].conjugate() * FROBENIUS_COEFFS[P - 1][1],
            f.coeffs[0].coeffs[2].conjugate() * FROBENIUS_COEFFS[P - 1][3],
        }),
        Fq6({
            f.coeffs[1].coeffs[0].conjugate() * FROBENIUS_COEFFS[P - 1][0],
            f.coeffs[1].coeffs[1].conjugate() * FROBENIUS_COEFFS[P - 1][2],
            f.coeffs[1].coeffs[2].conjugate() * FROBENIUS_COEFFS[P - 1][4],
        }),
    });
}

/// Computes Frobenius operator for Fq12 field member values
/// This specialization computes Frobenius^2
template <int P>
constexpr Fq12 endomorphism(const Fq12& f) noexcept
    requires(P == 2)
{
    return Fq12({
        Fq6({
            f.coeffs[0].coeffs[0],
            f.coeffs[0].coeffs[1] * FROBENIUS_COEFFS[1][1],
            f.coeffs[0].coeffs[2] * FROBENIUS_COEFFS[1][3],
        }),
        Fq6({
            f.coeffs[1].coeffs[0] * FROBENIUS_COEFFS[1][0],
            f.coeffs[1].coeffs[1] * FROBENIUS_COEFFS[1][2],
            f.coeffs[1].coeffs[2] * FROBENIUS_COEFFS[1][4],
        }),
    });
}


/// Computes `P0 + P1` in Jacobian coordinates.
constexpr ecc::JacPoint<Fq2> add(
    const ecc::JacPoint<Fq2>& P0, const ecc::JacPoint<Fq2>& P1) noexcept
{
    const auto& x0 = P0.x;
    const auto& y0 = P0.y;
    const auto& z0 = P0.z;

    const auto& x1 = P1.x;
    const auto& y1 = P1.y;
    const auto& z1 = P1.z;

    const auto z0_squared = z0 * z0;
    const auto z0_cubed = z0 * z0_squared;

    const auto z1_squared = z1 * z1;
    const auto z1_cubed = z1 * z1_squared;

    const auto U1 = x0 * z1_squared;
    const auto U2 = x1 * z0_squared;
    const auto S1 = y0 * z1_cubed;
    const auto S2 = y1 * z0_cubed;
    const auto H = U2 - U1;  // x1 * z0^2 - x0 * z1^2
    const auto R = S2 - S1;  // y1 * z0^3 - y0 * z1 ^3

    const auto H_squared = H * H;
    const auto H_cubed = H * H_squared;
    const auto R_squared = R * R;

    const auto V = U1 * H_squared;

    const auto X3 = R_squared - H_cubed - (V + V);
    const auto Y3 = R * (U1 * H_squared - X3) - S1 * H_cubed;
    const auto Z3 = H * z0 * z1;

    return {X3, Y3, Z3};
}

/// Computes `Q + Q` in Jacobian coordinates.
constexpr ecc::JacPoint<Fq2> dbl(const ecc::JacPoint<Fq2>& Q) noexcept
{
    const auto& x = Q.x;
    const auto& y = Q.y;
    const auto& z = Q.z;

    const auto y_squared = y * y;
    const auto x_squared = x * x;
    const auto z_squared = z * z;
    const auto y_4 = y_squared * y_squared;
    const auto _4y_4 = y_4 + y_4 + y_4 + y_4;

    const auto R = y_squared + y_squared;
    const auto A = (x + R);
    const auto S = A * A - x_squared - _4y_4;  // 2xR = (x+R)^2 - x^2 - R^2
    const auto M = x_squared + x_squared + x_squared;

    const auto N = y + z;

    const auto Xp = M * M - (S + S);
    const auto Yp = M * (S - Xp) - (_4y_4 + _4y_4);
    const auto Zp = N * N - y_squared - z_squared;  // 2yz = (y+z)^2 - y^2 - z^2

    return {Xp, Yp, Zp};
}

/// Computes `N` doubles of the point `a` in Jacobian coordinates.
template <int N>
constexpr ecc::JacPoint<Fq2> n_dbl(const ecc::JacPoint<Fq2>& a) noexcept
{
    auto r = dbl(a);
    for (int i = 0; i < N - 1; ++i)
        r = dbl(r);

    return r;
}

/// Addchain generated algorithm which multiplies point `a` in Jacobian coordinated
/// by X (curve seed).
constexpr ecc::JacPoint<Fq2> mul_by_X(const ecc::JacPoint<Fq2>& a) noexcept
{
    auto t0 = dbl(a);
    auto t2 = dbl(t0);
    auto c = dbl(t2);
    auto t4 = dbl(c);
    auto t6 = add(a, t4);
    t4 = add(t6, t0);
    auto t8 = add(a, t4);
    auto t10 = add(c, t6);
    auto t12 = dbl(t6);
    t8 = add(t8, t4);
    t4 = add(t8, t0);
    t12 = n_dbl<6>(t12);
    t2 = add(t12, t2);
    t2 = add(t2, t10);
    t2 = n_dbl<7>(t2);
    t10 = add(t2, t10);
    t10 = n_dbl<8>(t10);
    t10 = add(t10, t4);
    t0 = add(t10, t0);
    t0 = n_dbl<6>(t0);
    t6 = add(t0, t6);
    t6 = n_dbl<8>(t6);
    t6 = add(t6, t4);
    t6 = n_dbl<6>(t6);
    t6 = add(t6, t4);
    t6 = n_dbl<10>(t6);
    t8 = add(t6, t8);
    t8 = n_dbl<6>(t8);
    t4 = add(t4, t8);
    c = add(t4, c);

    return c;
}

/// Checks that point `p_aff` is in proper subgroup of points from twisted curve over Fq2 field.
/// For more details see https://eprint.iacr.org/2022/348.pdf Example 1 from 3.1.2 Examples
constexpr bool g2_subgroup_check(const ecc::Point<Fq2>& p_aff)
{
    const auto p = ecc::JacPoint<Fq2>::from(p_aff);

    const auto px = mul_by_X(p);
    const auto px1 = add(px, p);
    const auto _2px = dbl(px);

    const auto e_px = endomorphism<1>(px);
    const auto ee_px = endomorphism<1>(e_px);
    const auto eee_2px = endomorphism<1>(endomorphism<2>(_2px));

    const auto l = add(add(px1, e_px), ee_px);

    return l == eee_2px;
}

/// Computes point Q doubling for twisted curve + line tangent (in untwisted Q) to
/// the curve (not twisted curve) evaluated at point P
/// Returns live evaluation coefficients (-t, tw, tvw)
/// For more details see https://notes.ethereum.org/@ipsilon/Hkn2a2qk0
constexpr ecc::JacPoint<Fq2> lin_func_and_dbl(
    const ecc::JacPoint<Fq2>& Q, std::array<Fq2, 3>& t) noexcept
{
    const auto& x = Q.x;
    const auto& y = Q.y;
    const auto& z = Q.z;

    const auto y_squared = y * y;
    const auto x_squared = x * x;
    const auto z_squared = z * z;
    const auto y_4 = y_squared * y_squared;
    const auto _4y_4 = y_4 + y_4 + y_4 + y_4;

    const auto R = y_squared + y_squared;
    const auto A = (x + R);
    const auto S = A * A - x_squared - _4y_4;  // 2xR = (x+R)^2 - x^2 - R^2
    const auto M = x_squared + x_squared + x_squared;

    const auto N = y + z;

    const auto Xp = M * M - (S + S);
    const auto Yp = M * (S - Xp) - (_4y_4 + _4y_4);
    const auto Zp = N * N - y_squared - z_squared;  // 2yz = (y+z)^2 - y^2 - z^2

    t[0] = Zp * z_squared;
    t[1] = M * z_squared;
    t[2] = R - M * x;

    return ecc::JacPoint<Fq2>{Xp, Yp, Zp};
}

/// Computes points P0 and P1 addition for twisted curve + line defined by untwisted P1 and P2
/// points on the curve (not twisted curve) evaluated at point P. Formula is simplified for P1.z
/// == 1. For more details see https://notes.ethereum.org/@ipsilon/Hkn2a2qk0
[[nodiscard]] constexpr ecc::JacPoint<Fq2> lin_func_and_add(
    const ecc::JacPoint<Fq2>& P0, const ecc::Point<Fq2>& P1, std::array<Fq2, 3>& t) noexcept
{
    const auto& x0 = P0.x;
    const auto& y0 = P0.y;
    const auto& z0 = P0.z;

    const auto& x1 = P1.x;
    const auto& y1 = P1.y;

    const auto z0_squared = z0 * z0;
    const auto z0_cubed = z0 * z0_squared;

    const auto U2 = x1 * z0_squared;
    const auto S2 = y1 * z0_cubed;
    const auto H = U2 - x0;  // x1 * z0^2 - x0 * z1^2
    const auto R = S2 - y0;  // y1 * z0^3 - y0 * z1 ^3

    const auto H_squared = H * H;
    const auto H_cubed = H * H_squared;
    const auto R_squared = R * R;

    const auto V = x0 * H_squared;

    const auto X3 = R_squared - H_cubed - (V + V);
    const auto Y3 = R * (x0 * H_squared - X3) - y0 * H_cubed;
    const auto Z3 = H * z0;

    t[0] = (z0 * z0_squared * x0 - U2 * z0_cubed);
    t[1] = (S2 * z0_squared - y0 * z0_squared);
    t[2] = y0 * U2 - x0 * S2;

    return ecc::JacPoint<Fq2>{X3, Y3, Z3};
}

/// Computes points P0 and P1 addition for twisted curve + line defined by untwisted P1 and P2
/// points on the curve (not twisted curve) evaluated at point P. Formula is simplified for P1.z
/// == 1. For more details see https://notes.ethereum.org/@ipsilon/Hkn2a2qk0
constexpr void lin_func(
    const ecc::JacPoint<Fq2>& P0, const ecc::Point<Fq2>& P1, std::array<Fq2, 3>& t) noexcept
{
    const auto& x0 = P0.x;
    const auto& y0 = P0.y;
    const auto& z0 = P0.z;

    const auto& x1 = P1.x;
    const auto& y1 = P1.y;

    const auto z0_squared = z0 * z0;
    const auto z0_cubed = z0 * z0_squared;

    const auto U2 = x1 * z0_squared;
    const auto S2 = y1 * z0_cubed;

    t[0] = (z0 * z0_squared * x0 - U2 * z0_cubed);
    t[1] = (S2 * z0_squared - y0 * z0_squared);
    t[2] = y0 * U2 - x0 * S2;
}

/// Computes f^2 for f in Fq12. For more ref https://eprint.iacr.org/2010/354.pdf Algorithm 22
[[nodiscard]] constexpr Fq12 square(const Fq12& f) noexcept
{
    const Fq2& ksi = Fq6Config::ksi;

    const auto& a0 = f.coeffs[0];
    const auto& a1 = f.coeffs[1];
    auto c0 = a0 - a1;
    auto c3 = a0 - Fq6({ksi * a1.coeffs[2], a1.coeffs[0], a1.coeffs[1]});
    auto c2 = a0 * a1;
    c0 = c0 * c3 + c2;
    const auto c1 = c2 + c2;
    c2 = Fq6({ksi * c2.coeffs[2], c2.coeffs[0], c2.coeffs[1]});
    c0 = c0 + c2;

    return Fq12({c0, c1});
}

/// Computes `a^2` for `a` from `Fq^4 = Fq^2[V](V^2 - ksi)` where `V` is from Fq^2 extended field.
/// For more reference see https://eprint.iacr.org/2010/354.pdf Algorithm 9
constexpr std::pair<Fq2, Fq2> fq4_square(const std::pair<Fq2, Fq2>& a)
{
    const auto& a0 = a.first;
    const auto& a1 = a.second;

    const auto t0 = a0 * a0;
    const auto t1 = a1 * a1;

    const auto c0 = t1 * Fq6Config::ksi + t0;
    auto c1 = a0 + a1;
    c1 = c1 * c1 - t0 - t1;

    return {c0, c1};
}

/// Computes `c^2` for `x` from Fq^12 where `x^(FieldPrime^6 - 1) == 1`.
/// This is Fq^12 subgroup called cyclotomic polynomials or group of `r` roots of unity.
constexpr Fq12 cyclotomic_square(const Fq12& c)
{
    const auto& g = c.coeffs[0];
    const auto& h = c.coeffs[1];

    const auto& g0 = g.coeffs[0];
    const auto& g1 = g.coeffs[1];
    const auto& g2 = g.coeffs[2];

    const auto& h0 = h.coeffs[0];
    const auto& h1 = h.coeffs[1];
    const auto& h2 = h.coeffs[2];

    const auto [t00, t11] = fq4_square({g0, h1});
    const auto [t01, t12] = fq4_square({h0, g2});  // Typo in paper t01 <-> t12
    const auto [t02, aux] = fq4_square({g1, h2});

    const auto t10 = aux * Fq6Config::ksi;

    const auto c00 = (t00 + t00 + t00) - (g0 + g0);
    const auto c01 = (t01 + t01 + t01) - (g1 + g1);
    const auto c02 = (t02 + t02 + t02) - (g2 + g2);

    const auto c10 = h0 + h0 + t10 + t10 + t10;
    const auto c11 = h1 + h1 + t11 + t11 + t11;
    const auto c12 = h2 + h2 + t12 + t12 + t12;

    return Fq12({Fq6({c00, c01, c02}), Fq6({c10, c11, c12})});
}

/// Computes `cyclotomic_square` N times.
template <int N>
constexpr Fq12 n_cyclotomic_square(const Fq12& c)
{
    auto r = c;
    for (int i = 0; i < N; ++i)
        r = cyclotomic_square(r);

    return r;
}

/// Computes `a^X` where `X` is the curve seed parameter
/// and `a` is from cyclotomic subgroup of Fq^12.
constexpr Fq12 cyclotomic_pow_to_X(const Fq12& a)
{
    auto t0 = cyclotomic_square(a);
    auto t2 = cyclotomic_square(t0);
    auto c = cyclotomic_square(t2);
    auto t4 = cyclotomic_square(c);
    auto t6 = a * t4;
    t4 = t6 * t0;
    auto t8 = a * t4;
    auto t10 = c * t6;
    auto t12 = cyclotomic_square(t6);
    t8 = t8 * t4;
    t4 = t8 * t0;
    t12 = n_cyclotomic_square<6>(t12);
    t2 = t12 * t2;
    t2 = t2 * t10;
    t2 = n_cyclotomic_square<7>(t2);
    t10 = t2 * t10;
    t10 = n_cyclotomic_square<8>(t10);
    t10 = t10 * t4;
    t0 = t10 * t0;
    t0 = n_cyclotomic_square<6>(t0);
    t6 = t0 * t6;
    t6 = n_cyclotomic_square<8>(t6);
    t6 = t6 * t4;
    t6 = n_cyclotomic_square<6>(t6);
    t6 = t6 * t4;
    t6 = n_cyclotomic_square<10>(t6);
    t8 = t6 * t8;
    t8 = n_cyclotomic_square<6>(t8);
    t4 = t4 * t8;
    c = t4 * c;

    return c;
}

}  // namespace evmmax::bn254
