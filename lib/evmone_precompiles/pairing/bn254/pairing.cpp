// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../../bn254.hpp"
#include "fields.hpp"
#include "utils.hpp"
#include <span>
#include <vector>

namespace evmmax::bn254
{
namespace
{
/// Multiplies `fr` (Fq12) values by sparse `v` (Fq12) value of the form
/// [[t[0] * y, 0, 0],[t[1] * x, t[0], 0]] where `v` coefficients are from Fq2
constexpr void multiply_by_lin_func_value(
    Fq12& fr, std::array<Fq2, 3> t, const Fq& x, const Fq& y) noexcept
{
    const Fq12 f = fr;
    const auto& ksi = Fq6Config::ksi;

    const auto t0y = t[0] * y;
    const auto t1x = t[1] * x;
    const auto t2ksi = t[2] * ksi;

    fr.coeffs[0].coeffs[0] = f.coeffs[0].coeffs[0] * t0y + f.coeffs[1].coeffs[2] * t1x * ksi +
                             f.coeffs[1].coeffs[1] * t2ksi;
    fr.coeffs[0].coeffs[1] =
        f.coeffs[0].coeffs[1] * t0y + f.coeffs[1].coeffs[0] * t1x + f.coeffs[1].coeffs[2] * t2ksi;
    fr.coeffs[0].coeffs[2] =
        f.coeffs[0].coeffs[2] * t0y + f.coeffs[1].coeffs[1] * t1x + f.coeffs[1].coeffs[0] * t[2];
    fr.coeffs[1].coeffs[0] =
        f.coeffs[1].coeffs[0] * t0y + f.coeffs[0].coeffs[0] * t1x + f.coeffs[0].coeffs[2] * t2ksi;
    fr.coeffs[1].coeffs[1] =
        f.coeffs[1].coeffs[1] * t0y + f.coeffs[0].coeffs[1] * t1x + f.coeffs[0].coeffs[0] * t[2];
    fr.coeffs[1].coeffs[2] =
        f.coeffs[1].coeffs[2] * t0y + f.coeffs[0].coeffs[2] * t1x + f.coeffs[0].coeffs[1] * t[2];
}

// 0000000100010010000010000000010000100010000000010010000000001000000100100000010000000000100000100001001000000010001000000001000101
// NAF rep 00 -> 0, 01 -> 1, 10 -> -1
// miller loop goes from L-2 to 0 inclusively. NAF rep of 29793968203157093288 (6x+2) is two bits
// longer, but we omit lowest 2 bits.
inline constexpr auto ATE_LOOP_COUNT_NAF = 0x1120804220120081204008212022011_u128;
inline constexpr int LOG_ATE_LOOP_COUNT = 63;

/// State carried across iterations for one pair in the Miller loop.
struct MillerPairState
{
    ecc::JacPoint<Fq2> T;  ///< running Jacobian point on the twisted curve
    ecc::Point<Fq2> Q;     ///< the affine G2 input
    ecc::Point<Fq2> nQ;    ///< -Q, precomputed
    ecc::Point<Fq> P;      ///< the affine G1 input
    Fq ny;                 ///< -P.y, precomputed
};

/// Multi-pair Miller loop computing prod_i e_miller(P_i, Q_i).
///
/// Algorithm: https://eprint.iacr.org/2010/354.pdf Algorithm 1, batched across
/// all input pairs so the Fq12 squaring is shared instead of repeated per pair.
/// For N valid pairs, saves (N-1) × (LOG_ATE_LOOP_COUNT + 1) Fq12 squarings
/// vs. the per-pair-then-multiply approach.
Fq12 multi_miller_loop(std::span<MillerPairState> pairs) noexcept
{
    auto f = Fq12::one();
    std::array<Fq2, 3> t;
    auto naf = ATE_LOOP_COUNT_NAF;

    for (int i = 0; i <= LOG_ATE_LOOP_COUNT; ++i)
    {
        f = square(f);  // single squaring shared by every pair this iteration

        for (auto& s : pairs)
        {
            s.T = lin_func_and_dbl(s.T, t);
            multiply_by_lin_func_value(f, t, s.P.x, s.ny);
        }

        if (naf & 1)
        {
            for (auto& s : pairs)
            {
                s.T = lin_func_and_add(s.T, s.Q, t);
                multiply_by_lin_func_value(f, t, s.P.x, s.P.y);
            }
        }
        else if (naf & 2)
        {
            for (auto& s : pairs)
            {
                s.T = lin_func_and_add(s.T, s.nQ, t);
                multiply_by_lin_func_value(f, t, s.P.x, s.P.y);
            }
        }
        naf >>= 2;
    }

    // Post-loop Frobenius endomorphism steps, one set per pair.
    for (auto& s : pairs)
    {
        // untwist -> Frobenius -> twist
        const auto Q1 = endomorphism<1>(s.Q);
        // untwist -> Frobenius^2 -> twist, plus negation per Miller-loop spec
        const auto nQ2 = -endomorphism<2>(s.Q);

        s.T = lin_func_and_add(s.T, Q1, t);
        multiply_by_lin_func_value(f, t, s.P.x, s.P.y);

        lin_func(s.T, nQ2, t);
        multiply_by_lin_func_value(f, t, s.P.x, s.P.y);
    }

    return f;
}

/// Final exponentiation formula.
/// Based on https://eprint.iacr.org/2010/354.pdf 4.2 Algorithm 31.
Fq12 final_exp(const Fq12& v) noexcept
{
    auto f = v;
    auto f1 = f.conjugate();

    f = f1 * f.inv();            // easy 1
    f = endomorphism<2>(f) * f;  // easy 2

    f1 = f.conjugate();

    const auto ft1 = cyclotomic_pow_to_X(f);
    const auto ft2 = cyclotomic_pow_to_X(ft1);
    const auto ft3 = cyclotomic_pow_to_X(ft2);
    const auto fp1 = endomorphism<1>(f);
    const auto fp2 = endomorphism<2>(f);
    const auto fp3 = endomorphism<3>(f);
    const auto y0 = fp1 * fp2 * fp3;
    const auto y1 = f1;
    const auto y2 = endomorphism<2>(ft2);
    const auto y3 = endomorphism<1>(ft1).conjugate();
    const auto y4 = (endomorphism<1>(ft2) * ft1).conjugate();
    const auto y5 = ft2.conjugate();
    const auto y6 = (endomorphism<1>(ft3) * ft3).conjugate();

    auto t0 = cyclotomic_square(y6) * y4 * y5;
    auto t1 = y3 * y5 * t0;
    t0 = t0 * y2;
    t1 = cyclotomic_square(t1) * t0;
    t1 = cyclotomic_square(t1);
    t0 = t1 * y1;
    t1 = t1 * y0;
    t0 = cyclotomic_square(t0);
    return t1 * t0;
}
}  // namespace

std::optional<bool> pairing_check(std::span<const std::pair<Point, ExtPoint>> pairs) noexcept
{
    if (pairs.empty())
        return true;

    std::vector<MillerPairState> states;
    states.reserve(pairs.size());

    for (const auto& [p, q] : pairs)
    {
        if (!is_field_element(p.x) || !is_field_element(p.y) || !is_field_element(q.x.first) ||
            !is_field_element(q.x.second) || !is_field_element(q.y.first) ||
            !is_field_element(q.y.second))
        {
            return std::nullopt;
        }

        // Converts points' coefficients in Montgomery form.
        const auto P_aff = ecc::Point<Fq>{Fq(p.x), Fq(p.y)};
        const auto Q_aff = ecc::Point<Fq2>{
            Fq2({Fq(q.x.first), Fq(q.x.second)}), Fq2({Fq(q.y.first), Fq(q.y.second)})};

        const bool g1_is_inf = is_infinity(P_aff);
        const bool g2_is_inf = g2_is_infinity(Q_aff);

        // Verify that P in on curve. For this group it also means that P is in G1.
        if (!g1_is_inf && !is_on_curve(P_aff))
            return std::nullopt;

        // Verify that Q in on curve and in proper subgroup. This subgroup is much smaller than
        // group containing all the points from twisted curve over Fq2 field.
        if (!g2_is_inf && (!is_on_twisted_curve(Q_aff) || !g2_subgroup_check(Q_aff)))
            return std::nullopt;

        // Skip pairs where either point is at infinity — they contribute 1 to the product.
        if (!g1_is_inf && !g2_is_inf)
            states.push_back({ecc::JacPoint<Fq2>::from(Q_aff), Q_aff, -Q_aff, P_aff, -P_aff.y});
    }

    if (states.empty())
        return true;

    const auto f = multi_miller_loop(states);
    return final_exp(f) == Fq12::one();
}
}  // namespace evmmax::bn254
