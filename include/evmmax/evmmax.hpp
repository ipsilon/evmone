// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <intx/intx.hpp>

namespace evmmax
{

/// Compute the modulus inverse for Montgomery multiplication, i.e. N': mod⋅N' = 2⁶⁴-1.
///
/// @param mod0  The least significant word of the modulus.
constexpr uint64_t compute_mod_inv(uint64_t mod0) noexcept
{
    // TODO: Find what is this algorithm and why it works.
    uint64_t base = 0 - mod0;
    uint64_t result = 1;
    for (auto i = 0; i < 64; ++i)
    {
        result *= base;
        base *= base;
    }
    return result;
}

/// The modular arithmetic operations for EVMMAX (EVM Modular Arithmetic Extensions).
template <typename UintT>
class ModArith
{
public:
    const UintT mod;  ///< The modulus.

private:
    const UintT m_r_squared;  ///< R² % mod.

    /// The modulus inversion, i.e. the number N' such that mod⋅N' = 2⁶⁴-1.
    const uint64_t m_mod_inv;

    /// Compute R² % mod.
    static constexpr UintT compute_r_squared(const UintT& mod) noexcept
    {
        // R is 2^num_bits, R² is 2^(2*num_bits) and needs 2*num_bits+1 bits to represent,
        // rounded to 2*num_bits+64) for intx requirements.
        constexpr auto r2 = intx::uint<UintT::num_bits * 2 + 64>{1} << (UintT::num_bits * 2);
        return intx::udivrem(r2, mod).rem;
    }

    static constexpr std::pair<uint64_t, uint64_t> addmul(
        uint64_t t, uint64_t a, uint64_t b, uint64_t c) noexcept
    {
        const auto p = intx::umul(a, b) + t + c;
        return {p[1], p[0]};
    }

public:
    constexpr explicit ModArith(const UintT& modulus) noexcept
      : mod{modulus},
        m_r_squared{compute_r_squared(modulus)},
        m_mod_inv{compute_mod_inv(modulus[0])}
    {}

    /// Converts a value to Montgomery form.
    ///
    /// This is done by using Montgomery multiplication mul(x, R²)
    /// what gives aR²R⁻¹ % mod = aR % mod.
    constexpr UintT to_mont(const UintT& x) const noexcept { return mul(x, m_r_squared); }

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

            const auto m = t[0] * m_mod_inv;
            std::tie(c, std::ignore) = addmul(t[0], m, mod[0], 0);
#pragma GCC unroll 8
            for (size_t j = 1; j != S; ++j)
                std::tie(c, t[j - 1]) = addmul(t[j], m, mod[j], c);
            tmp = intx::addc(t[S], c);
            t[S - 1] = tmp.value;
            t[S] = d + tmp.carry;  // TODO: Carry is 0 for sparse modulus.
        }

        if (t >= mod)
            t -= mod;

        return static_cast<UintT>(t);
    }

    /// Performs a modular addition. It is required that x < mod and y < mod, but x and y may be
    /// but are not required to be in Montgomery form.
    constexpr UintT add(const UintT& x, const UintT& y) const noexcept
    {
        const auto s = addc(x, y);  // TODO: cannot overflow if modulus is sparse (e.g. 255 bits).
        const auto d = subc(s.value, mod);
        return (!s.carry && d.carry) ? s.value : d.value;
    }

    /// Performs a modular subtraction. It is required that x < mod and y < mod, but x and y may be
    /// but are not required to be in Montgomery form.
    constexpr UintT sub(const UintT& x, const UintT& y) const noexcept
    {
        const auto d = subc(x, y);
        const auto s = d.value + mod;
        return (d.carry) ? s : d.value;
    }

    /// Compute the modular inversion of the x in Montgomery form. The result is in Montgomery form.
    /// If x is not invertible, the result is 0.
    constexpr UintT inv(const UintT& x) const noexcept
    {
        assert((mod & 1) == 1);
        assert(mod >= 3);

        // Precompute inverse of 2 modulo mod: inv2 * 2 % mod == 1.
        // The 1/2 is inexact division which can be fixed by adding "0" to the numerator
        // and making it even: (mod + 1) / 2. To avoid potential overflow of (1 + mod)
        // we rewrite it further to (mod - 1 + 2) / 2 = (mod - 1) / 2 + 1 = ⌊mod / 2⌋ + 1.
        const auto inv2 = (mod >> 1) + 1;

        // Use extended binary Euclidean algorithm. This evolves variables a and b until a is 0.
        // Then GCD(x, mod) is in b. If GCD(x, mod) == 1 then the inversion exists and is in v.
        // This follows the classic algorithm (Algorithm 1) presented in
        // "Optimized Binary GCD for Modular Inversion".
        // https://eprint.iacr.org/2020/972.pdf#algorithm.1
        // TODO: The same paper has additional optimizations that could be applied.
        UintT a = x;
        UintT b = mod;

        // Bézout's coefficients are originally initialized to 1 and 0. But because the input x
        // is in Montgomery form XR the algorithm would compute X⁻¹R⁻¹. To get the expected X⁻¹R,
        // we need to multiply the result by R². We can achieve the same effect "for free"
        // by initializing u to R² instead of 1.
        UintT u = m_r_squared;
        UintT v = 0;

        while (a != 0)
        {
            if ((a & 1) != 0)
            {
                // if a is odd update it to a - b.
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

            // Compute u / 2 % mod. If u is even this can be computed as ⌊u / 2⌋.
            // Otherwise, (u - 1 + 1) / 2 = ⌊u / 2⌋ + (1 / 2 % mod).
            const auto u_odd = (u & 1) != 0;
            u >>= 1;
            if (u_odd)
                u += inv2;  // if u is odd add back ½ % mod.
        }

        if (b != 1)
            return 0;  // not invertible
        return v;
    }
};
}  // namespace evmmax
