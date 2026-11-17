// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <intx/intx.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <type_traits>
#include <utility>

namespace evmone::crypto
{
/// Computes the modular inverse of a modulo 2³², satisfying a⋅inv = 1 mod 2³².
/// Only odd arguments are invertible modulo a power of two.
constexpr uint32_t modinv_pow2(uint32_t a) noexcept
{
    assert(a % 2 == 1);  // The argument must be odd, otherwise the inverse does not exist.

    // Start with inversion mod 2⁴, which is a³ mod 2⁴ (for odd a).
    // All 8 cases can be verified manually, but the formal explanation can be found in:
    // https://en.wikipedia.org/wiki/Multiplicative_group_of_integers_modulo_n#Powers_of_2.
    // This is better tradeoff than a mod 2² plus one Newton-Raphson iteration.
    // We also avoid explicit mod 2⁴ because the top garbage bits are fine.
    auto inv = a * a * a;

    // Use the Newton–Raphson numeric method, see e.g.
    // https://gmplib.org/~tege/divcnst-pldi94.pdf#page=9, formula (9.2)
    // Each iteration doubles the number of correct bits, starting from 4:
    // 8, 16, 32, ..., so for 32-bit value we need 3 iterations.
    // TODO(C++23): static
    constexpr auto ITERATIONS = std::countr_zero(sizeof(a) * 8 / 4);
    for (auto i = 0; i < ITERATIONS; ++i)
        inv *= 2 - a * inv;  // Overflows are fine because they wrap around modulo 2³².

    assert(inv * a == 1);  // Verify the result.
    return inv;
}

/// Computes the modular inverse of a modulo 2⁶⁴, satisfying a⋅inv = 1 mod 2⁶⁴.
/// Only odd arguments are invertible modulo a power of two.
constexpr uint64_t modinv_pow2(uint64_t a) noexcept
{
    assert(a % 2 == 1);  // The argument must be odd, otherwise the inverse does not exist.
    uint64_t inv = modinv_pow2(static_cast<uint32_t>(a));  // Start with inversion mod 2³².
    inv *= 2 - a * inv;    // One Newton-Raphson iteration: 64 bits correct.
    assert(inv * a == 1);  // Verify the result.
    return inv;
}

/// Compute the modulus inverse for Montgomery multiplication, i.e., N': mod⋅N' = 2⁶⁴-1.
template <typename UintT>
constexpr uint64_t compute_mont_mod_inv(const UintT& mod) noexcept
{
    // Compute the inversion mod[0]⁻¹ mod 2⁶⁴, then the final result is N' = -mod[0]⁻¹
    // because this gives mod⋅N' = -1 mod 2⁶⁴ = 2⁶⁴-1.
    return -modinv_pow2(mod[0]);
}

/// Computes a⋅b + t + c, which always fits in 128 bits. Returns {high word, low word}.
constexpr std::pair<uint64_t, uint64_t> addmul(
    uint64_t t, uint64_t a, uint64_t b, uint64_t c) noexcept
{
    // TODO(GCC): The intx::uint128 sum is kept in a stack slot.
#if INTX_HAS_BUILTIN_INT128 && !defined(__clang__)
    const auto p = intx::builtin_uint128{a} * b + t + c;
    return {static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    const auto p = intx::umul(a, b) + t + c;
    return {p[1], p[0]};
#endif
}

/// Performs a modular addition. Requires x < mod and y < mod.
template <typename UintT>
constexpr UintT modadd(const UintT& x, const UintT& y, const UintT& mod) noexcept
{
    assert(x < mod);
    assert(y < mod);
    const auto s = addc(x, y);  // TODO: cannot overflow if modulus is sparse (e.g. 255 bits).
    const auto d = subc(s.value, mod);
    return (!s.carry && d.carry) ? s.value : d.value;
}

/// Performs a modular subtraction. Requires x < mod and y < mod.
template <typename UintT>
constexpr UintT modsub(const UintT& x, const UintT& y, const UintT& mod) noexcept
{
    assert(x < mod);
    assert(y < mod);
    const auto d = subc(x, y);
    const auto s = d.value + mod;
    return (d.carry) ? s : d.value;
}

// The building blocks of modinv_scaled(): the safegcd algorithm of D. J. Bernstein and B.-Y. Yang,
// "Fast constant-time gcd computation and modular inversion", IACR TCHES 2019(3),
// https://eprint.iacr.org/2019/266 (the section and theorem numbers refer to its 2019-04-13
// version). As in §12.1, batches of 62 divsteps run on the low bits, and their transition matrix
// is then applied to the full values. The code departs from the paper's recip2 (§11) in:
// - Variable time: it stops once g = 0 instead of after the fixed number of divsteps of
//   Theorem 11.2.
// - A batch does the divsteps of all trailing zeros of g at once, and cancels the low k ≤ 5 bits
//   of g at once by adding w⋅f, w ≡ −g⋅f⁻¹ (mod 2ᵏ).
// - The inverse is the top-right entry vₙ of the product of the matrices, as in recip2, but each
//   batch's matrix updates the right column [vₙ, rₙ] modulo mod and divides it by 2⁶², instead of
//   a precomputed power of 1/2 at the end.
// - The values are signed 62-bit limbs, and f and g drop their top limb once it only extends the
//   sign.
// - rₙ starts as scale, and non-invertible x gives 0.
// The first four follow libsecp256k1's modinv64_var (which cancels up to 6 bits after a swap and
// 4 bits otherwise), as does the correction that keeps vₙ and rₙ in (−2⋅mod, mod), and this code
// owes much to its explanation in
// https://github.com/bitcoin-core/secp256k1/blob/master/doc/safegcd_implementation.md.
namespace safegcd
{
using intx::int128;
using intx::smul;

/// The number of divsteps in a batch, which is also the number of bits of a limb.
constexpr int BATCH = 62;

/// The mask of the bits of a limb.
constexpr uint64_t LIMB_MASK = (uint64_t{1} << BATCH) - 1;

/// A signed integer in limbs of 62 bits, the least significant first. All limbs but the top one
/// are in [0, 2⁶²).
template <size_t N>
using Limbs = std::array<int64_t, N>;

/// The transition matrix of a batch of divsteps scaled by 2⁶² (§12.1):
/// [u v; q r]⋅[f, g] = 2⁶²⋅[f', g'] for the f' and g' after the batch.
/// The row sums |u| + |v| and |q| + |r| are at most 2⁶².
struct TransitionMatrix
{
    int64_t u;
    int64_t v;
    int64_t q;
    int64_t r;
};

/// Runs BATCH divsteps (§8) from delta and the low 62 bits of f and g, f odd.
/// Updates delta and returns the transition matrix of the batch.
[[gnu::noinline]] constexpr TransitionMatrix divsteps(
    int64_t& delta, uint64_t f, uint64_t g) noexcept
{
    assert((f & 1) == 1);

    // The matrix of the divsteps so far, scaled by 2ⁱ after i of them:
    // u⋅f₀ + v⋅g₀ = 2ⁱ⋅f and q⋅f₀ + r⋅g₀ = 2ⁱ⋅g.
    int64_t u = 1;
    int64_t v = 0;
    int64_t q = 0;
    int64_t r = 1;
    int left = BATCH;
    while (true)
    {
        // While g is even, a divstep is (δ, f, g) → (1 + δ, f, g/2).
        const auto zeros = std::countr_zero(g | (~uint64_t{0} << left));
        g >>= zeros;
        u <<= zeros;
        v <<= zeros;
        delta += zeros;
        left -= zeros;
        if (left == 0)
            break;

        // For odd g, a divstep is the conditional swap (δ, f, g) → (−δ, g, −f) if δ > 0,
        // followed by the elimination (δ, f, g) → (1 + δ, f, (g + f)/2) (§8.2).
        // neg_g is −g, kept apart to take the negation off the critical path.
        auto neg_g = -g;
        if (delta > 0)
        {
            delta = -delta;
            neg_g = f;
            f = g;
            g = -neg_g;
            std::swap(u, q);
            std::swap(v, r);
            q = -q;
            r = -r;
        }

        // Now δ ≤ 0, so the next 1 − δ divsteps are eliminations, also those for even g.
        // The first k = min(1 − δ, left, 5) of them add w⋅f to g, for w ≡ −g⋅f⁻¹ (mod 2ᵏ),
        // which gives g k trailing zeros for the shift above. f⁻¹ ≡ (3⋅f) ⊕ 2 (mod 2⁵), as the
        // 16 odd residues show, so w takes a single multiplication.
        const auto k = std::min<int64_t>(1 - delta, left);
        const auto w = (neg_g * ((3 * f) ^ 2)) & ~(~uint64_t{0} << k) & 31;
        g += w * f;
        q += static_cast<int64_t>(w) * u;
        r += static_cast<int64_t>(w) * v;
    }
    // The matrices T(δ, f, g) of §8.1 have determinant 1/2,
    // so the matrix 2⁶²⋅T⋯T of the batch has determinant 2⁶².
    assert(smul(u, r) - smul(v, q) == int128{1} << BATCH);
    assert((u < 0 ? -u : u) + (v < 0 ? -v : v) <= int64_t{1} << BATCH);
    assert((q < 0 ? -q : q) + (r < 0 ? -r : r) <= int64_t{1} << BATCH);
    return {u, v, q, r};
}

/// The low limb of x.
constexpr int64_t low_limb(const int128& x) noexcept
{
    return static_cast<int64_t>(x & LIMB_MASK);
}

/// Computes [f, g] = t⋅[f, g] / 2⁶² for f and g of len limbs and the matrix t of the divsteps from
/// their low limbs, which makes the division exact and the results fit in len limbs (§8.3).
template <size_t N>
[[gnu::always_inline]] constexpr void update_fg(
    size_t len, Limbs<N>& f, Limbs<N>& g, const TransitionMatrix& t) noexcept
{
    const auto [u, v, q, r] = t;
    auto cf = smul(u, f[0]) + smul(v, g[0]);
    auto cg = smul(q, f[0]) + smul(r, g[0]);
    assert(low_limb(cf) == 0 && low_limb(cg) == 0);
    cf >>= BATCH;
    cg >>= BATCH;
    for (size_t i = 1; i < len; ++i)
    {
        cf += smul(u, f[i]) + smul(v, g[i]);
        cg += smul(q, f[i]) + smul(r, g[i]);
        f[i - 1] = low_limb(cf);
        g[i - 1] = low_limb(cg);
        cf >>= BATCH;
        cg >>= BATCH;
    }
    f[len - 1] = static_cast<int64_t>(cf);
    g[len - 1] = static_cast<int64_t>(cg);
}

/// Computes [vn, rn] = (t⋅[vn, rn] + mod⋅[mv, mr]) / 2⁶², with mv and mr making the division
/// exact. Requires vn and rn in (−2⋅mod, mod), the row sums of t at most 2⁶², and
/// mod_inv⋅mod ≡ 1 (mod 2⁶⁴). The results are in (−2⋅mod, mod).
template <size_t N>
[[gnu::always_inline]] constexpr void update_vr(Limbs<N>& vn, Limbs<N>& rn,
    const TransitionMatrix& t, const Limbs<N>& mod, uint64_t mod_inv) noexcept
{
    const auto [u, v, q, r] = t;

    // Counting negative vn or rn as itself plus mod keeps |u⋅vn + v⋅rn| < 2⁶²⋅mod,
    // then adding less than 2⁶² more multiples of mod makes the division exact
    // and puts the result in (−2⋅mod, mod).
    const auto vn_sign = vn[N - 1] >> 63;
    const auto rn_sign = rn[N - 1] >> 63;
    auto mv = (u & vn_sign) + (v & rn_sign);
    auto mr = (q & vn_sign) + (r & rn_sign);
    auto cv = smul(u, vn[0]) + smul(v, rn[0]);
    auto cr = smul(q, vn[0]) + smul(r, rn[0]);

    // Subtract from mv the residue modulo 2⁶² that cancels the low limb of cv + mod⋅mv.
    const auto cancel = [mod_inv](const int128& c, int64_t m) noexcept {
        const auto residue = (mod_inv * static_cast<uint64_t>(c) + static_cast<uint64_t>(m));
        return m - static_cast<int64_t>(residue & LIMB_MASK);
    };
    mv = cancel(cv, mv);
    mr = cancel(cr, mr);
    cv += smul(mod[0], mv);
    cr += smul(mod[0], mr);
    assert(low_limb(cv) == 0 && low_limb(cr) == 0);
    cv >>= BATCH;
    cr >>= BATCH;
    for (size_t i = 1; i < N; ++i)
    {
        cv += smul(u, vn[i]) + smul(v, rn[i]) + smul(mod[i], mv);
        cr += smul(q, vn[i]) + smul(r, rn[i]) + smul(mod[i], mr);
        vn[i - 1] = low_limb(cv);
        rn[i - 1] = low_limb(cr);
        cv >>= BATCH;
        cr >>= BATCH;
    }
    vn[N - 1] = static_cast<int64_t>(cv);
    rn[N - 1] = static_cast<int64_t>(cr);
}

/// Splits the non-negative x into N limbs.
template <size_t N, typename UintT>
constexpr Limbs<N> to_limbs(const UintT& x) noexcept
{
    Limbs<N> r{};
    for (size_t i = 0; i < N; ++i)
        r[i] = static_cast<int64_t>((x >> (i * BATCH))[0] & LIMB_MASK);
    return r;
}

/// The value of the first len limbs of a.
template <typename SintT, size_t N>
constexpr SintT from_limbs(const Limbs<N>& a, size_t len) noexcept
{
    SintT r = a[len - 1];
    for (size_t i = len - 1; i != 0; --i)
        r = (r << BATCH) | SintT{a[i - 1]};
    return r;
}

template <typename UintT>
constexpr UintT modinv_scaled(const UintT& x, const UintT& scale, const UintT& mod) noexcept
{
    assert((mod & 1) == 1);
    assert(mod >= 3);
    assert(x < mod);
    assert(scale < mod);  // Otherwise rn starts out of its range.

    // f and g are in [−mod, mod], vn and rn in (−2⋅mod, mod): 2 bits more than UintT.
    constexpr auto N = (UintT::num_bits + 2 + BATCH - 1) / BATCH;
    const auto m = to_limbs<N>(mod);
    const auto mod_inv = modinv_pow2(mod[0]);

    // (δ, f, g) = divstepⁿ(1, mod, x) as in Theorem 11.2, with vn and rn, the right column of the
    // product of the transition matrices, kept modulo mod and times scale:
    // vn⋅x ≡ scale⋅f and rn⋅x ≡ scale⋅g (mod mod).
    int64_t delta = 1;
    auto f = m;
    auto g = to_limbs<N>(x);
    Limbs<N> vn{};
    auto rn = to_limbs<N>(scale);
    size_t len = N;  // The limbs of f and g in use, the higher limbs are stale.

    // For b-bit f and g, b ≥ 46, g = 0 after ⌊(49⋅b + 57)/17⌋ divsteps (Theorem 11.2).
    [[maybe_unused]] constexpr auto MAX_DIVSTEPS = (49 * UintT::num_bits + 57) / 17;
    [[maybe_unused]] unsigned num_divsteps = 0;
    while (true)
    {
        assert(num_divsteps < MAX_DIVSTEPS);
        num_divsteps += BATCH;
        const auto t = divsteps(delta, static_cast<uint64_t>(f[0]), static_cast<uint64_t>(g[0]));
        update_fg(len, f, g, t);
        update_vr(vn, rn, t, m, mod_inv);

        int64_t g_any = 0;
        for (size_t i = 0; i < len; ++i)
            g_any |= g[i];
        if (g_any == 0)
            break;

        // Drop the top limbs of f and g if both are 0 or −1: the limbs below take the sign.
        const auto f_top = f[len - 1];
        const auto g_top = g[len - 1];
        if (len >= 2 && (f_top == 0 || f_top == -1) && (g_top == 0 || g_top == -1))
        {
            f[len - 2] += f_top * (int64_t{1} << BATCH);
            g[len - 2] += g_top * (int64_t{1} << BATCH);
            --len;
        }
    }

    // Now f = ±gcd(x, mod), and vn ≡ ±scale⋅x⁻¹ (mod mod) if the gcd is 1.
    using SintT = intx::sint<UintT::num_bits + 64>;
    const auto f_value = from_limbs<SintT>(f, len);
    if (f_value != 1 && f_value != -1)
        return 0;

    // Move vn from (−2⋅mod, mod) to [0, mod), negated if f = −1.
    const auto mod_value = from_limbs<SintT>(m, N);
    auto vn_value = from_limbs<SintT>(vn, N);
    if (vn_value < 0)
        vn_value += mod_value;
    if (f_value == -1)
        vn_value = -vn_value;
    if (vn_value < 0)
        vn_value += mod_value;
    assert(vn_value >= 0 && vn_value < mod_value);
    return static_cast<UintT>(static_cast<intx::uint<SintT::num_bits>>(vn_value));
}

extern template intx::uint256 modinv_scaled(
    const intx::uint256& x, const intx::uint256& scale, const intx::uint256& mod) noexcept;
extern template intx::uint384 modinv_scaled(
    const intx::uint384& x, const intx::uint384& scale, const intx::uint384& mod) noexcept;

/// The runtime modinv_scaled(), defined in modarith.cpp for intx::uint256 and intx::uint384.
template <typename UintT>
UintT modinv_scaled_rt(const UintT& x, const UintT& scale, const UintT& mod) noexcept;
}  // namespace safegcd

/// Computes scale⋅x⁻¹ % mod, i.e. the modular inverse of x scaled by the given factor.
/// Returns 0 for non-invertible x (including x == 0).
/// Requires an odd mod not less than 3, and both x and scale less than mod.
/// Runs in variable time. At runtime, defined for intx::uint256 and intx::uint384.
template <typename UintT>
constexpr UintT modinv_scaled(const UintT& x, const UintT& scale, const UintT& mod) noexcept
{
    if (std::is_constant_evaluated())
        return safegcd::modinv_scaled(x, scale, mod);
    return safegcd::modinv_scaled_rt(x, scale, mod);
}

/// The modular arithmetic operations using the Montgomery form of the values.
template <typename UintT>
class ModArith
{
    const UintT mod_;  ///< The modulus.

    const UintT r_squared_;  ///< R² % mod.

    /// The modulus inversion, i.e. the number N' such that mod⋅N' = 2⁶⁴-1.
    const uint64_t mod_inv_;

    /// Compute R² % mod.
    static constexpr UintT compute_r_squared(const UintT& mod) noexcept
    {
        // R is 2^num_bits, R² is 2^(2*num_bits) and needs 2*num_bits+1 bits to represent,
        // rounded to 2*num_bits+64 for intx requirements.
        constexpr auto RR = intx::uint<UintT::num_bits * 2 + 64>{1} << (UintT::num_bits * 2);
        return intx::udivrem(RR, mod).rem;
    }

public:
    constexpr explicit ModArith(const UintT& mod) noexcept
      : mod_{mod}, r_squared_{compute_r_squared(mod)}, mod_inv_{compute_mont_mod_inv(mod)}
    {}

    /// Returns the modulus.
    constexpr const UintT& mod() const noexcept { return mod_; }

    /// Converts a value to Montgomery form.
    ///
    /// This is done by using Montgomery multiplication mul(x, R²)
    /// what gives aR²R⁻¹ % mod = aR % mod.
    constexpr UintT to_mont(const UintT& x) const noexcept { return mul(x, r_squared_); }

    /// Converts a value in Montgomery form back to normal value.
    ///
    /// Given the x is the Montgomery form x = aR, the conversion is done by using
    /// Montgomery multiplication mul(x, 1) what gives aRR⁻¹ % mod = a % mod.
    constexpr UintT from_mont(const UintT& x) const noexcept { return mul(x, 1); }

    /// Performs a Montgomery modular multiplication.
    ///
    /// Inputs must be in Montgomery form: x = aR, y = bR.
    /// This computes Montgomery multiplication xyR⁻¹ % mod what gives aRbRR⁻¹ % mod = abR % mod.
    /// The result (abR) is in Montgomery form.
    constexpr UintT mul(const UintT& x, const UintT& y) const noexcept
    {
        // Coarsely Integrated Operand Scanning (CIOS) Method
        // Based on 2.3.2 from
        // High-Speed Algorithms & Architectures For Number-Theoretic Cryptosystems
        // https://www.microsoft.com/en-us/research/wp-content/uploads/1998/06/97Acar.pdf

        constexpr auto S = UintT::num_words;  // TODO(C++23): Make it static

        intx::uint<UintT::num_bits + 64> t;
        for (size_t i = 0; i != S; ++i)
        {
            uint64_t c = 0;
#pragma GCC unroll 8
            for (size_t j = 0; j != S; ++j)
                std::tie(c, t[j]) = addmul(t[j], x[j], y[i], c);
            auto tmp = intx::addc(t[S], c);
            t[S] = tmp.value;
            const auto d = tmp.carry;  // TODO: Carry is 0 for sparse modulus.

            const auto m = t[0] * mod_inv_;
            std::tie(c, std::ignore) = addmul(t[0], m, mod_[0], 0);
#pragma GCC unroll 8
            for (size_t j = 1; j != S; ++j)
                std::tie(c, t[j - 1]) = addmul(t[j], m, mod_[j], c);
            tmp = intx::addc(t[S], c);
            t[S - 1] = tmp.value;
            t[S] = d + tmp.carry;  // TODO: Carry is 0 for sparse modulus.
        }

        if (t >= mod_)
            t -= mod_;

        return static_cast<UintT>(t);
    }

    /// Performs a modular addition.
    constexpr UintT add(const UintT& x, const UintT& y) const noexcept
    {
        // Using generic procedure is fine for Montgomery forms.
        return modadd(x, y, mod_);
    }

    /// Performs a modular subtraction.
    constexpr UintT sub(const UintT& x, const UintT& y) const noexcept
    {
        // Using generic procedure is fine for Montgomery forms.
        return modsub(x, y, mod_);
    }

    /// Computes modular inverse of x in Montgomery form. Result is in Montgomery form.
    /// Returns 0 for non-invertible x (including x == 0).
    constexpr UintT inv(const UintT& x) const noexcept
    {
        // The input XR would invert to X⁻¹R⁻¹, so scaling by R² gives the expected Montgomery
        // form X⁻¹R at no extra cost.
        return modinv_scaled(x, r_squared_, mod_);
    }
};
}  // namespace evmone::crypto
