// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modarith.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>

namespace evmone::crypto
{
// The building blocks of modinv_scaled(): the safegcd algorithm of D. J. Bernstein and B.-Y. Yang,
// "Fast constant-time gcd computation and modular inversion", IACR TCHES 2019(3),
// https://eprint.iacr.org/2019/266 (the theorem numbers refer to its 2019-04-13 version),
// in the variable-time form of libsecp256k1's modinv64_var,
// https://github.com/bitcoin-core/secp256k1/blob/master/doc/safegcd_implementation.md.
// It differs from modinv64_var in:
// - The values are signed integers of N+1 64-bit words in two's complement, N the words of UintT,
//   instead of 62-bit limbs. This works for any width, and a batch is 61 divsteps instead of 62.
// - divsteps() cancels up to 5 bits of g with one formula, instead of up to 6 bits after a swap
//   and 4 bits otherwise.
// - modinv_scaled() starts with e = scale instead of 1 and returns 0 for non-invertible x.
namespace
{
/// The number of divsteps in a batch.
constexpr int BATCH = 61;
// The word sums of update_de() fit in 128 bits because |u| + |v| + |md| < 3⋅2ᴮᴬᵀᶜᴴ < 2⁶³.
// This simple bound limits BATCH to 61.
static_assert(3 * (uint64_t{1} << BATCH) < uint64_t{1} << 63);

/// The transition matrix of BATCH divsteps: 2ᴮᴬᵀᶜᴴ⋅[f', g'] = [u v; q r]⋅[f, g].
/// The row sums |u| + |v| and |q| + |r| are at most 2ᴮᴬᵀᶜᴴ.
struct TransitionMatrix
{
    int64_t u;
    int64_t v;
    int64_t q;
    int64_t r;
};

/// Computes a⋅b as a two's complement 128-bit value.
intx::uint128 mul_su(int64_t a, uint64_t b) noexcept
{
    auto p = intx::umul(static_cast<uint64_t>(a), b);
    p[1] -= b & static_cast<uint64_t>(a >> 63);
    return p;
}

/// Computes a⋅b as a two's complement 128-bit value.
intx::uint128 mul_ss(int64_t a, int64_t b) noexcept
{
    auto p = intx::umul(static_cast<uint64_t>(a), static_cast<uint64_t>(b));
    p[1] -= (static_cast<uint64_t>(b) & static_cast<uint64_t>(a >> 63)) +
            (static_cast<uint64_t>(a) & static_cast<uint64_t>(b >> 63));
    return p;
}

/// Shifts the two's complement 128-bit x right by 64 bits, keeping the sign.
intx::uint128 sar64(const intx::uint128& x) noexcept
{
    return {x[1], static_cast<uint64_t>(static_cast<int64_t>(x[1]) >> 63)};
}

/// Sign-extends x to 128 bits.
intx::uint128 sext(int64_t x) noexcept
{
    return sar64({0, static_cast<uint64_t>(x)});
}

/// Returns the low word of the 128-bit [hi, lo] shifted right by BATCH bits.
uint64_t shr_batch(uint64_t lo, uint64_t hi) noexcept
{
    return (lo >> BATCH) | (hi << (64 - BATCH));
}

/// Applies BATCH divsteps to f and g given by their low words, f odd.
/// Updates eta = -delta and returns the transition matrix.
TransitionMatrix divsteps(int64_t& eta, uint64_t f0, uint64_t g0) noexcept
{
    assert((f0 & 1) == 1);

    // Each iteration does the divsteps for the trailing zeros of g at once, then swaps f and g
    // if delta > 0 and cancels up to 5 low bits of g by adding a multiple of f, but no more bits
    // than the divsteps until the next swap or the end of the batch.
    // The matrix is kept modulo 2⁶⁴.
    uint64_t f = f0;
    uint64_t g = g0;
    uint64_t u = 1;
    uint64_t v = 0;
    uint64_t q = 0;
    uint64_t r = 1;
    int steps_left = BATCH;
    while (true)
    {
        const auto zeros = std::countr_zero(g | (~uint64_t{0} << steps_left));
        g >>= zeros;
        u <<= zeros;
        v <<= zeros;
        eta -= zeros;
        steps_left -= zeros;
        if (steps_left == 0)
            break;

        // -g, after a swap the old f: this keeps a negation off the critical path.
        auto neg_g = -g;
        if (eta < 0)
        {
            eta = -eta;
            neg_g = f;
            f = g;
            g = -neg_g;
            const auto u0 = u;
            u = q;
            q = -u0;
            const auto v0 = v;
            v = r;
            r = -v0;
        }
        const auto limit = std::min(static_cast<int>(eta) + 1, steps_left);
        // f⁻¹ ≡ (3⋅f) ⊕ 2 (mod 2⁵), so w needs a single multiplication.
        const auto mask = (~uint64_t{0} >> (64 - limit)) & 31;
        const auto w = (neg_g * ((3 * f) ^ 2)) & mask;
        g += f * w;
        assert((g & mask) == 0);
        q += u * w;
        r += v * w;
    }
    assert(u * f0 + v * g0 == f << BATCH);
    assert(q * f0 + r * g0 == g << BATCH);
    const TransitionMatrix t{static_cast<int64_t>(u), static_cast<int64_t>(v),
        static_cast<int64_t>(q), static_cast<int64_t>(r)};
    assert(std::abs(t.u) + std::abs(t.v) <= int64_t{1} << BATCH);
    assert(std::abs(t.q) + std::abs(t.r) <= int64_t{1} << BATCH);
    return t;
}

/// Computes [f, g] = t⋅[f, g] / 2ᴮᴬᵀᶜᴴ for f and g of len words, the division is exact.
/// The words from len on are neither read nor written.
template <typename SignedT>
void update_fg(size_t len, SignedT& f, SignedT& g, const TransitionMatrix& t) noexcept
{
    const auto [u, v, q, r] = t;
    intx::uint128 sum_f;
    intx::uint128 sum_g;
    uint64_t prev_f = 0;  // The low word of the previous partial sum.
    uint64_t prev_g = 0;
    for (size_t i = 0; i < len - 1; ++i)
    {
        sum_f += mul_su(u, f[i]) + mul_su(v, g[i]);
        sum_g += mul_su(q, f[i]) + mul_su(r, g[i]);
        if (i != 0)
        {
            f[i - 1] = shr_batch(prev_f, sum_f[0]);
            g[i - 1] = shr_batch(prev_g, sum_g[0]);
        }
        prev_f = sum_f[0];
        prev_g = sum_g[0];
        sum_f = sar64(sum_f);
        sum_g = sar64(sum_g);
    }
    const auto f_top = static_cast<int64_t>(f[len - 1]);
    const auto g_top = static_cast<int64_t>(g[len - 1]);
    sum_f += mul_ss(u, f_top) + mul_ss(v, g_top);
    sum_g += mul_ss(q, f_top) + mul_ss(r, g_top);
    // f and g stay in [-2ⁿ, 2ⁿ) for n = 64⋅len - 1 (§8.3), so the results fit in len words.
    assert(static_cast<int64_t>(sum_f[1]) >> (BATCH - 1) == static_cast<int64_t>(sum_f[1]) >> 63);
    assert(static_cast<int64_t>(sum_g[1]) >> (BATCH - 1) == static_cast<int64_t>(sum_g[1]) >> 63);
    if (len != 1)
    {
        f[len - 2] = shr_batch(prev_f, sum_f[0]);
        g[len - 2] = shr_batch(prev_g, sum_g[0]);
    }
    f[len - 1] = shr_batch(sum_f[0], sum_f[1]);
    g[len - 1] = shr_batch(sum_g[0], sum_g[1]);
}

/// Computes [d, e] = (t⋅[d, e] + mod⋅[md, me]) / 2ᴮᴬᵀᶜᴴ, with md and me making the division
/// exact. Requires d and e in (-2⋅mod, mod) and mod_inv⋅mod ≡ 1 (mod 2⁶⁴).
/// The results are in (-2⋅mod, mod).
template <typename SignedT, typename UintT>
void update_de(
    SignedT& d, SignedT& e, const TransitionMatrix& t, const UintT& mod, uint64_t mod_inv) noexcept
{
    constexpr auto TOP = UintT::num_words;
    constexpr uint64_t MASK = (uint64_t{1} << BATCH) - 1;
    const auto [u, v, q, r] = t;
    // Start with md and me adding mod for each negative d or e: this keeps the range.
    const auto d_sign = static_cast<int64_t>(d[TOP]) >> 63;
    const auto e_sign = static_cast<int64_t>(e[TOP]) >> 63;
    auto md = (u & d_sign) + (v & e_sign);
    auto me = (q & d_sign) + (r & e_sign);
    auto sum_d = mul_su(u, d[0]) + mul_su(v, e[0]);
    auto sum_e = mul_su(q, d[0]) + mul_su(r, e[0]);
    md -= static_cast<int64_t>((mod_inv * sum_d[0] + static_cast<uint64_t>(md)) & MASK);
    me -= static_cast<int64_t>((mod_inv * sum_e[0] + static_cast<uint64_t>(me)) & MASK);
    sum_d += mul_su(md, mod[0]);
    sum_e += mul_su(me, mod[0]);
    assert((sum_d[0] & MASK) == 0);
    assert((sum_e[0] & MASK) == 0);
    auto prev_d = sum_d[0];  // The low word of the previous partial sum.
    auto prev_e = sum_e[0];
    sum_d = sar64(sum_d);
    sum_e = sar64(sum_e);
    for (size_t i = 1; i < TOP; ++i)
    {
        sum_d += mul_su(u, d[i]) + mul_su(v, e[i]) + mul_su(md, mod[i]);
        sum_e += mul_su(q, d[i]) + mul_su(r, e[i]) + mul_su(me, mod[i]);
        d[i - 1] = shr_batch(prev_d, sum_d[0]);
        e[i - 1] = shr_batch(prev_e, sum_e[0]);
        prev_d = sum_d[0];
        prev_e = sum_e[0];
        sum_d = sar64(sum_d);
        sum_e = sar64(sum_e);
    }
    // The top words are in [-2, 0], so the products fit in int64.
    const auto d_top = static_cast<int64_t>(d[TOP]);
    const auto e_top = static_cast<int64_t>(e[TOP]);
    assert(d_top >= -2 && d_top <= 0);
    assert(e_top >= -2 && e_top <= 0);
    sum_d += sext(u * d_top + v * e_top);
    sum_e += sext(q * d_top + r * e_top);
    d[TOP - 1] = shr_batch(prev_d, sum_d[0]);
    e[TOP - 1] = shr_batch(prev_e, sum_e[0]);
    d[TOP] = shr_batch(sum_d[0], sum_d[1]);
    e[TOP] = shr_batch(sum_e[0], sum_e[1]);
}
}  // namespace

template <typename UintT>
UintT modinv_scaled(const UintT& x, const UintT& scale, const UintT& mod) noexcept
{
    assert((mod & 1) == 1);
    assert(mod >= 3);
    assert(x < mod);
    assert(scale < mod);  // Otherwise e starts out of its range.

    using SignedT = intx::uint<UintT::num_bits + 64>;

    // Keeps d⋅x ≡ scale⋅f and e⋅x ≡ scale⋅g (mod mod) while the divsteps drive g to 0.
    // The scale is the initial e and the algorithm is linear in it,
    // so any factor can be folded into the result for free.
    const auto mod_inv = modinv_pow2(mod[0]);
    SignedT d = 0;
    SignedT e = scale;
    SignedT f = mod;
    SignedT g = x;
    const SignedT signed_mod = mod;
    int64_t eta = -1;                 // eta = -delta, delta starts at 1.
    size_t len = SignedT::num_words;  // The words of f and g in use, the higher words are stale.

    // For b-bit f and g, b ≥ 46, g = 0 after ⌊(49⋅b + 57)/17⌋ divsteps (Theorem 11.2).
    [[maybe_unused]] constexpr auto MAX_DIVSTEPS = (49 * UintT::num_bits + 57) / 17;
    [[maybe_unused]] unsigned num_divsteps = 0;
    while (true)
    {
        assert(num_divsteps < MAX_DIVSTEPS);
        num_divsteps += BATCH;
        const auto t = divsteps(eta, f[0], g[0]);
        update_fg(len, f, g, t);
        update_de(d, e, t, mod, mod_inv);
        assert(intx::slt(-(2 * signed_mod), d) && intx::slt(d, signed_mod));
        assert(intx::slt(-(2 * signed_mod), e) && intx::slt(e, signed_mod));

        uint64_t g_any = 0;
        for (size_t i = 0; i < len; ++i)
            g_any |= g[i];
        if (g_any == 0)
            break;

        // Drop the top word if it only extends the sign of the word below.
        if (len >= 2)
        {
            const auto f_ext = static_cast<uint64_t>(static_cast<int64_t>(f[len - 2]) >> 63);
            const auto g_ext = static_cast<uint64_t>(static_cast<int64_t>(g[len - 2]) >> 63);
            if (((f[len - 1] ^ f_ext) | (g[len - 1] ^ g_ext)) == 0)
                --len;
        }
    }

    // Now f = ±gcd(x, mod), and d ≡ f⋅scale⋅x⁻¹ (mod mod) if the gcd is 1.
    const auto f_sign = static_cast<uint64_t>(static_cast<int64_t>(f[len - 1]) >> 63);
    if (f[0] != (f_sign | 1))
        return 0;
    for (size_t i = 1; i < len; ++i)
    {
        if (f[i] != f_sign)
            return 0;
    }

    // Move d from (-2⋅mod, mod) to [0, mod), negated if f = -1.
    if (static_cast<int64_t>(d[SignedT::num_words - 1]) < 0)
        d += signed_mod;
    if (f_sign != 0)
        d = -d;
    if (static_cast<int64_t>(d[SignedT::num_words - 1]) < 0)
        d += signed_mod;
    assert(d < signed_mod);
    return static_cast<UintT>(d);
}

template intx::uint256 modinv_scaled(
    const intx::uint256& x, const intx::uint256& scale, const intx::uint256& mod) noexcept;
template intx::uint384 modinv_scaled(
    const intx::uint384& x, const intx::uint384& scale, const intx::uint384& mod) noexcept;
}  // namespace evmone::crypto
