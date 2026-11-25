// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <intx/intx.hpp>
#include <cassert>

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
    const auto p = intx::mac(t, a, b, c);
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

// The safegcd values are signed integers, shifted and masked in two's complement (C++20).
// NOLINTBEGIN(*-signed-bitwise)

/// The helpers of modinv_scaled().
/// Its comments describe the algorithm and cite the paper that the § numbers refer to.
namespace safegcd
{
using intx::int128;
using intx::smul;

/// The number of divsteps in a batch, which is also the number of bits of a limb.
constexpr int BATCH = 62;

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
/// Returns the transition matrix of the batch and the updated delta.
[[gnu::noinline]] constexpr std::pair<TransitionMatrix, int> divsteps(
    int delta, uint64_t f, uint64_t g) noexcept
{
    assert(f % 2 == 1);

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
        // With k = min(1 − δ, left), the first j = min(k, 5) of them add multiplier⋅f to g, for
        // multiplier ≡ −g⋅f⁻¹ (mod 2ʲ), which gives g j trailing zeros for the shift above.
        // f⁻¹ ≡ (3⋅f) ⊕ 2 (mod 2⁵) for all 16 odd residues, so multiplier takes one multiplication.
        const auto k = std::min(1 - delta, left);
        const auto multiplier = (neg_g * ((3 * f) ^ 2)) % (uint64_t{1} << k) % 32;
        g += multiplier * f;
        assert(g % (uint64_t{1} << std::min(k, 5)) == 0);
        q += static_cast<int64_t>(multiplier) * u;
        r += static_cast<int64_t>(multiplier) * v;
    }
    // The matrices T(δ, f, g) of §8.1 have determinant 1/2,
    // so the matrix 2⁶²⋅T⋯T of the batch has determinant 2⁶².
    assert(smul(u, r) - smul(v, q) == int128{uint64_t{1} << BATCH});
    assert(smul(u, u < 0 ? -1 : 1) + smul(v, v < 0 ? -1 : 1) <= int128{uint64_t{1} << BATCH});
    assert(smul(q, q < 0 ? -1 : 1) + smul(r, r < 0 ? -1 : 1) <= int128{uint64_t{1} << BATCH});
    return {{u, v, q, r}, delta};
}

constexpr int64_t low_limb(const int128& x) noexcept
{
    return static_cast<int64_t>(x[0] & LIMB_MASK);
}

/// Computes [f, g] = t⋅[f, g] / 2⁶² for the first size limbs of f and g, with t the matrix of the
/// divsteps of their low limbs: the division is exact and the results fit in size limbs (§8.3).
template <size_t N>
[[gnu::always_inline]] constexpr void update_fg(
    Limbs<N>& f, Limbs<N>& g, size_t size, const TransitionMatrix& t) noexcept
{
    const auto [u, v, q, r] = t;
    auto cf = smul(u, f[0]) + smul(v, g[0]);
    auto cg = smul(q, f[0]) + smul(r, g[0]);
    assert(low_limb(cf) == 0 && low_limb(cg) == 0);
    cf >>= BATCH;
    cg >>= BATCH;
    for (size_t i = 1; i < size; ++i)
    {
        cf += smul(u, f[i]) + smul(v, g[i]);
        cg += smul(q, f[i]) + smul(r, g[i]);
        f[i - 1] = low_limb(cf);
        g[i - 1] = low_limb(cg);
        cf >>= BATCH;
        cg >>= BATCH;
    }
    f[size - 1] = static_cast<int64_t>(cf);
    g[size - 1] = static_cast<int64_t>(cg);
}

/// Computes [vn, rn] = (t⋅[vn, rn] + mod⋅[mv, mr]) / 2⁶², with mv and mr making the division
/// exact. Requires vn and rn in (−2⋅mod, mod), the row sums of t at most 2⁶², and
/// mod_inv⋅mod ≡ 1 (mod 2⁶⁴). The results are in (−2⋅mod, mod).
template <size_t N>
[[gnu::always_inline]] constexpr void update_vr(Limbs<N>& vn, Limbs<N>& rn,
    const TransitionMatrix& t, const Limbs<N>& mod, uint64_t mod_inv) noexcept
{
    const auto [u, v, q, r] = t;

    // Counting negative vn or rn as itself plus mod keeps |u⋅vn + v⋅rn| < 2⁶²⋅mod; subtracting
    // less than 2⁶² multiples of mod then makes the division exact, with results in (−2⋅mod, mod).
    const auto vn_sign = vn[N - 1] >> 63;
    const auto rn_sign = rn[N - 1] >> 63;
    auto mv = (u & vn_sign) + (v & rn_sign);
    auto mr = (q & vn_sign) + (r & rn_sign);
    auto cv = smul(u, vn[0]) + smul(v, rn[0]);
    auto cr = smul(q, vn[0]) + smul(r, rn[0]);

    // Subtract from m the residue modulo 2⁶² that cancels the low limb of c + mod⋅m.
    const auto cancel = [mod_inv](const int128& c, int64_t m) noexcept {
        const auto residue = mod_inv * c[0] + static_cast<uint64_t>(m);
        return m - static_cast<int64_t>(residue & LIMB_MASK);
    };
    mv = cancel(cv, mv);
    mr = cancel(cr, mr);
    cv += smul(mod[0], mv);
    cr += smul(mod[0], mr);
    assert(low_limb(cv) == 0 && low_limb(cr) == 0);
    cv >>= BATCH;
    cr >>= BATCH;
    // TODO(gcc-14): Unroll by the template-dependent count N.
#pragma GCC unroll 8
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

/// Splits x into N limbs.
template <size_t N, typename UintT>
constexpr Limbs<N> to_limbs(const UintT& x) noexcept
{
    Limbs<N> r{};
    for (size_t i = 0; i < N; ++i)
        r[i] = static_cast<int64_t>((x >> (i * BATCH))[0] & LIMB_MASK);
    return r;
}

/// Returns the value of the first size limbs of a.
template <typename SintT, size_t N>
constexpr SintT from_limbs(const Limbs<N>& a, size_t size) noexcept
{
    SintT r = a[size - 1];
    for (size_t i = size - 1; i != 0; --i)
        r = (r << BATCH) | SintT{a[i - 1]};
    return r;
}

}  // namespace safegcd

/// Computes scale⋅x⁻¹ % mod, i.e. the modular inverse of x scaled by the given factor.
/// Returns 0 for non-invertible x (including x == 0).
/// Requires an odd mod not less than 3, and both x and scale less than mod.
/// Runs in variable time.
///
/// Implements the safegcd algorithm of D. J. Bernstein and B.-Y. Yang,
/// "Fast constant-time gcd computation and modular inversion", IACR TCHES 2019(3),
/// https://eprint.iacr.org/2019/266 (section and theorem numbers of its 2019-04-13 version).
/// The comments in the function body describe the implementation details.
template <typename UintT>
constexpr UintT modinv_scaled(const UintT& x, const UintT& scale, const UintT& mod) noexcept
{
    // As in §12.1, batches of 62 divsteps run on the low bits, and their transition matrix is
    // then applied to the full values. The code departs from the paper's recip2 (§11) in:
    // 1. It stops at g = 0, not after the divsteps count of Theorem 11.2.
    // 2. A batch does the divsteps of all trailing zeros of g at once, and cancels up to 5 low
    //    bits of g by adding a multiple of f.
    // 3. The inverse is the top-right vₙ of the matrix product, as in recip2, but each batch
    //    updates [vₙ, rₙ] modulo mod and divides it by 2⁶², not by a power of 1/2 at the end.
    // 4. The values are signed integers in 62-bit limbs, and f and g drop a top limb that only
    //    extends the sign.
    // 5. rₙ starts as scale, and the gcd is checked.
    //
    // The first four, and the correction that keeps vₙ and rₙ in (−2⋅mod, mod), follow
    // libsecp256k1's modinv64_var (which cancels up to 6 bits after a swap and 4 otherwise), see
    // https://github.com/bitcoin-core/secp256k1/blob/master/doc/safegcd_implementation.md.

    using namespace safegcd;

    assert((mod & 1) == 1);
    assert(mod >= 3);
    assert(x < mod);
    assert(scale < mod);  // Otherwise rn starts out of its range.

    // f and g are in [−mod, mod], vn and rn in (−2⋅mod, mod): 2 bits more than UintT.
    constexpr auto N = (UintT::num_bits + 2 + BATCH - 1) / BATCH;
    const auto mod_limbs = to_limbs<N>(mod);
    const auto mod_inv = modinv_pow2(mod[0]);

    // (δ, f, g) = divstepⁿ(1, mod, x) (Theorem 11.2), vn⋅x ≡ scale⋅f and rn⋅x ≡ scale⋅g (mod mod).
    int delta = 1;
    auto f = mod_limbs;
    auto g = to_limbs<N>(x);
    Limbs<N> vn{};
    auto rn = to_limbs<N>(scale);
    size_t fg_size = N;  // The limbs of f and g in use, the higher limbs are stale.

    using SintT = intx::sint<UintT::num_bits + 64>;
    [[maybe_unused]] const auto in_fg_range = [&mod](const Limbs<N>& a, size_t size) noexcept {
        const auto value = from_limbs<SintT>(a, size);
        return value + mod >= 0 && value <= mod;
    };
    [[maybe_unused]] const auto in_vr_range = [&mod](const Limbs<N>& a) noexcept {
        const auto value = from_limbs<SintT>(a, N);
        return value + mod + mod > 0 && value < mod;
    };

    // For |f|, |g| < 2ᵈ, d ≥ 46, g = 0 after ⌊(49⋅d + 57)/17⌋ divsteps (Theorem 11.2).
    [[maybe_unused]] constexpr auto ITERATIONS = (49 * UintT::num_bits + 57) / 17;
    for ([[maybe_unused]] unsigned num_divsteps = 0;; num_divsteps += BATCH)
    {
        assert(num_divsteps < ITERATIONS);
        const auto [t, new_delta] =
            divsteps(delta, static_cast<uint64_t>(f[0]), static_cast<uint64_t>(g[0]));
        delta = new_delta;
        update_fg(f, g, fg_size, t);
        assert(in_fg_range(f, fg_size) && in_fg_range(g, fg_size));
        update_vr(vn, rn, t, mod_limbs, mod_inv);
        assert(in_vr_range(vn) && in_vr_range(rn));

        int64_t g_any = 0;
        for (size_t i = 0; i < fg_size; ++i)
            g_any |= g[i];
        if (g_any == 0)
            break;

        // Drop the top limbs of f and g if both are 0 or −1: the limbs below take the sign.
        const auto f_top = f[fg_size - 1];
        const auto g_top = g[fg_size - 1];
        if (fg_size >= 2 && (f_top == 0 || f_top == -1) && (g_top == 0 || g_top == -1))
        {
            f[fg_size - 2] += f_top << BATCH;
            g[fg_size - 2] += g_top << BATCH;
            --fg_size;
        }
    }

    // Now f = ±gcd(x, mod), and vn ≡ ±scale⋅x⁻¹ (mod mod) if the gcd is 1.
    // TODO: For a prime mod the sign of f would do (the gcd is 1, or x = 0 and vn = 0). Specialize,
    //   e.g. with an is_prime template parameter, unless a compile-time ModArith modulus covers it.
    const auto f_value = from_limbs<SintT>(f, fg_size);
    if (f_value != 1 && f_value != -1) [[unlikely]]
        return 0;

    // Move vn from (−2⋅mod, mod) to [0, mod), negated if f = −1.
    auto vn_value = from_limbs<SintT>(vn, N);
    if (vn_value < 0)
        vn_value += mod;
    if (f_value == -1)
        vn_value = -vn_value;
    if (vn_value < 0)
        vn_value += mod;
    assert(vn_value >= 0 && vn_value < mod);
    return static_cast<UintT>(vn_value);
}
// NOLINTEND(*-signed-bitwise)

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
