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
/// [[t[0] * y, 0, 0],[t[1] * x, t[2], 0]] where `v` coefficients are from Fq2
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

/// The per-pair state carried across the Miller loop iterations.
struct MillerPairState
{
    ecc::ProjPoint<E2> T;       ///< The running point on the twisted curve.
    ecc::AffinePoint<E2> Q;     ///< The G2 input point.
    ecc::AffinePoint<E2> nQ;    ///< -Q.
    ecc::AffinePoint<Curve> P;  ///< The G1 input point.
    Fq ny;                      ///< -P.y.
};

/// Miller loop for all the pairs at once,
/// according to https://eprint.iacr.org/2010/354.pdf Algorithm 1.
///
/// The recurrence f ← f²·line is multiplicative across pairs, so one accumulator serves all of
/// them and the Fq12 squaring is paid once per iteration instead of once per iteration per pair.
Fq12 multi_miller_loop(std::span<MillerPairState> states) noexcept
{
    auto f = Fq12::one();
    std::array<Fq2, 3> t;
    auto naf = ATE_LOOP_COUNT_NAF;

    for (int i = 0; i <= LOG_ATE_LOOP_COUNT; ++i)
    {
        f = square(f);

        for (auto& s : states)
        {
            s.T = lin_func_and_dbl(s.T, t);
            multiply_by_lin_func_value(f, t, s.P.x, s.ny);
        }

        if (naf & 1)
        {
            for (auto& s : states)
            {
                s.T = lin_func_and_add(s.T, s.Q, t);
                multiply_by_lin_func_value(f, t, s.P.x, s.P.y);
            }
        }
        else if (naf & 2)
        {
            for (auto& s : states)
            {
                s.T = lin_func_and_add(s.T, s.nQ, t);
                multiply_by_lin_func_value(f, t, s.P.x, s.P.y);
            }
        }
        naf >>= 2;
    }

    for (auto& s : states)
    {
        // Frobenius endomorphism for point Q from twisted curve over Fq2 field.
        // It's essentially untwist -> frobenius -> twist chain of transformation.
        const auto Q1 = endomorphism<1>(s.Q);

        // Similar to above one. It makes untwist -> frobenius^2 -> twist transformation plus
        // negation according to miller loop spec.
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

std::optional<bool> pairing_check(std::span<const std::pair<AffinePoint, ExtPoint>> pairs) noexcept
{
    if (pairs.empty())
        return true;

    std::vector<MillerPairState> states;
    states.reserve(pairs.size());

    for (const auto& [p, q] : pairs)
    {
        if (!validate(p))
            return std::nullopt;

        const bool g2_is_inf = q == 0;

        // Verify that Q is on the curve and in the proper subgroup. This subgroup is much smaller
        // than the group containing all the points from the twisted curve over Fq2 field.
        if (!g2_is_inf && (!is_on_twisted_curve(q) || !g2_subgroup_check(q)))
            return std::nullopt;

        // If either point is infinity, the pair contributes 1 to the product, so skip it.
        if (p != 0 && !g2_is_inf)
            states.push_back({ecc::ProjPoint<E2>{q}, q, -q, p, -p.y});
    }

    // With every pair skipped the product is 1, and final_exp(1) == 1.
    if (states.empty())
        return true;

    // final exp is calculated on accumulated value
    return final_exp(multi_miller_loop(states)) == Fq12::one();
}
}  // namespace evmmax::bn254
