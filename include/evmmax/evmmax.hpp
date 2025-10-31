// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <intx/intx.hpp>
#include <cassert>

namespace evmmax
{
/// Compute the modular inverse of the number modulo 2⁶⁴: inv⋅a = 1 mod 2⁶⁴.
constexpr uint64_t inv_mod(uint64_t a) noexcept
{
    assert(a % 2 == 1);  // The argument must be odd, otherwise the inverse does not exist.

    // Use the Newton–Raphson numeric method, see e.g.
    // https://gmplib.org/~tege/divcnst-pldi94.pdf#page=9, formula (9.2)
    // Each iteration doubles the number of correct bits:
    // 2, 4, 8, ..., so for 64-bit value we need 6 iterations.
    // TODO(C++23): static
    constexpr auto ITERATIONS = std::countr_zero(sizeof(a) * 8);

    // Start with inversion mod 2.
    // TODO: This can be further accelerated by:
    //   - computing the inversion with smaller type (e.g. uint32_t) first,
    //   - using a better initial approximation (e.g. via lookup table for 4 bits).
    uint64_t inv = 1;
    for (auto i = 0; i < ITERATIONS; ++i)
        inv *= 2 - a * inv;  // Overflows are fine because they wrap around modulo 2⁶⁴.
    return inv;
}

/// The modular arithmetic operations for EVMMAX (EVM Modular Arithmetic Extensions).
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

    /// Compute the modulus inverse for Montgomery multiplication, i.e., N': mod⋅N' = 2⁶⁴-1.
    static constexpr uint64_t compute_mont_mod_inv(const UintT& mod) noexcept
    {
        // Compute the inversion mod[0]⁻¹ mod 2⁶⁴, then the final result is N' = -mod[0]⁻¹
        // because this gives mod⋅N' = -1 mod 2⁶⁴ = 2⁶⁴-1.
        return -inv_mod(mod[0]);
    }

    static constexpr std::pair<uint64_t, uint64_t> addmul(
        uint64_t t, uint64_t a, uint64_t b, uint64_t c) noexcept
    {
        const auto p = intx::umul(a, b) + t + c;
        return {p[1], p[0]};
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

    /// Performs a modular addition. It is required that x < mod and y < mod, but x and y may be
    /// but are not required to be in Montgomery form.
    constexpr UintT add(const UintT& x, const UintT& y) const noexcept
    {
        const auto s = addc(x, y);  // TODO: cannot overflow if modulus is sparse (e.g. 255 bits).
        const auto d = subc(s.value, mod_);
        return (!s.carry && d.carry) ? s.value : d.value;
    }

    /// Performs a modular subtraction. It is required that x < mod and y < mod, but x and y may be
    /// but are not required to be in Montgomery form.
    constexpr UintT sub(const UintT& x, const UintT& y) const noexcept
    {
        const auto d = subc(x, y);
        const auto s = d.value + mod_;
        return (d.carry) ? s : d.value;
    }

    /// Compute the modular inversion of the x in Montgomery form. The result is in Montgomery form.
    /// If x is not invertible, the result is 0.
    constexpr UintT inv(const UintT& x) const noexcept
    {
        assert((mod_ & 1) == 1);
        assert(mod_ >= 3);

        // Precompute inverse of 2 modulo mod: inv2 * 2 % mod == 1.
        // The 1/2 is inexact division that can be fixed by adding "0" to the numerator
        // and making it even: (mod + 1) / 2. To avoid potential overflow of (1 + mod)
        // we rewrite it further to (mod - 1 + 2) / 2 = (mod - 1) / 2 + 1 = ⌊mod / 2⌋ + 1.
        const auto inv2 = (mod_ >> 1) + 1;

        // Use extended binary Euclidean algorithm. This evolves variables a and b until a is 0.
        // Then GCD(x, mod) is in b. If GCD(x, mod) == 1 then the inversion exists and is in v.
        // This follows the classic algorithm (Algorithm 1) presented in
        // "Optimized Binary GCD for Modular Inversion".
        // https://eprint.iacr.org/2020/972.pdf#algorithm.1
        // TODO: The same paper has additional optimizations that could be applied.
        UintT a = x;
        UintT b = mod_;

        // Bézout's coefficients are originally initialized to 1 and 0. But because the input x
        // is in Montgomery form XR the algorithm would compute X⁻¹R⁻¹. To get the expected X⁻¹R,
        // we need to multiply the result by R². We can achieve the same effect "for free"
        // by initializing u to R² instead of 1.
        UintT u = r_squared_;
        UintT v = 0;

        while (a != 0)
        {
            if ((a & 1) != 0)
            {
                // if a is odd, update it to a - b.
                if (const auto [d, less] = subc(a, b); less)
                {
                    // swap a and b in case a < b.
                    b = a;
                    a = -d;

                    using namespace std;
                    swap(u, v);
                }
                else
                {
                    a = d;
                }
                u = sub(u, v);
            }

            // Compute a / 2 % mod, a is even so division is exact and can be computed as ⌊a / 2⌋.
            a >>= 1;

            // Compute u / 2 % mod. If u is even, this can be computed as ⌊u / 2⌋.
            // Otherwise, (u - 1 + 1) / 2 = ⌊u / 2⌋ + (1 / 2 % mod).
            const auto u_odd = (u & 1) != 0;
            u >>= 1;
            if (u_odd)
                u += inv2;  // if u is odd, add back ½ % mod.
        }

        if (b != 1) [[unlikely]]
            v = 0;  // not invertible
        return v;
    }
};
}  // namespace evmmax
