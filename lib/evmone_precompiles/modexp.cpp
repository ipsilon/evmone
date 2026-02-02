// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modexp.hpp"
#include <evmmax/evmmax.hpp>
#include <bit>

using namespace intx;

namespace
{
/// Represents the exponent value of the modular exponentiation operation.
///
/// This is a view type of the big-endian bytes representing the bits of the exponent.
class Exponent
{
    const uint8_t* data_ = nullptr;
    size_t bit_width_ = 0;

public:
    explicit Exponent(std::span<const uint8_t> bytes) noexcept
    {
        const auto it = std::ranges::find_if(bytes, [](auto x) { return x != 0; });
        const auto trimmed_bytes = std::span{it, bytes.end()};
        bit_width_ = trimmed_bytes.empty() ? 0 :
                                             static_cast<size_t>(std::bit_width(trimmed_bytes[0])) +
                                                 (trimmed_bytes.size() - 1) * 8;
        data_ = trimmed_bytes.data();
    }


    [[nodiscard]] size_t bit_width() const noexcept { return bit_width_; }

    /// Returns the bit value of the exponent at the given index, counting from the most significant
    /// bit (e[0] is the top bit).
    bool operator[](size_t index) const noexcept
    {
        // TODO: Replace this with a custom iterator type.
        const auto exp_size = (bit_width_ + 7) / 8;
        const auto byte_index = index / 8;
        const auto byte = data_[exp_size - 1 - byte_index];
        const auto bit_index = index % 8;
        const auto bit = (byte >> bit_index) & 1;
        return bit != 0;
    }
};

/// Performs a Montgomery modular multiplication.
///
/// Inputs must be in Montgomery form: x = aR, y = bR.
/// This computes Montgomery multiplication xyR⁻¹ % mod what gives aRbRR⁻¹ % mod = abR % mod.
/// The result (abR) is in Montgomery form.
template <typename UintT>
constexpr UintT mul_mont(
    const UintT& x, const UintT& y, const UintT& mod, uint64_t mod_inv) noexcept
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
            std::tie(c, t[j]) = evmmax::addmul(t[j], x[j], y[i], c);
        const auto [sum1, d1] = intx::addc(t[S], c);
        t[S] = sum1;

        const auto m = t[0] * mod_inv;
        std::tie(c, std::ignore) = evmmax::addmul(t[0], m, mod[0], 0);
#pragma GCC unroll 8
        for (size_t j = 1; j != S; ++j)
            std::tie(c, t[j - 1]) = evmmax::addmul(t[j], m, mod[j], c);
        const auto [sum2, d2] = intx::addc(t[S], c);
        t[S - 1] = sum2;
        t[S] = d1 + d2;
    }

    if (t >= mod)
        t -= mod;

    return static_cast<UintT>(t);
}

template <typename UIntT>
UIntT modexp_odd(const UIntT& base, Exponent exp, const UIntT& mod) noexcept
{
    assert(exp.bit_width() != 0);  // Exponent of zero must be handled outside.

    const auto mod_inv = evmmax::compute_mont_mod_inv(mod);

    /// Convert the base to Montgomery form: base*R % mod, where R = 2^(num_bits).
    const auto base_mont =
        udivrem(intx::uint<UIntT::num_bits * 2>{base} << UIntT::num_bits, mod).rem;

    auto ret_mont = base_mont;
    for (auto i = exp.bit_width() - 1; i != 0; --i)
    {
        ret_mont = mul_mont(ret_mont, ret_mont, mod, mod_inv);
        if (exp[i - 1])
            ret_mont = mul_mont(ret_mont, base_mont, mod, mod_inv);
    }

    // Convert the result from the Montgomery form (reuse mul_mont with neutral factor 1).
    const auto ret = mul_mont(ret_mont, UIntT{1}, mod, mod_inv);
    return ret;
}

template <typename UIntT>
UIntT modexp_pow2(const UIntT& base, Exponent exp, unsigned k) noexcept
{
    assert(k != 0);  // Modulus of 1 should be covered as "odd".
    UIntT ret = 1;
    for (auto i = exp.bit_width(); i != 0; --i)
    {
        ret *= ret;
        if (exp[i - 1])
            ret *= base;
    }

    const auto mod_pow2_mask = (UIntT{1} << k) - 1;
    ret &= mod_pow2_mask;
    return ret;
}

/// Computes modular inversion for modulus of 2ᵏ.
///
/// TODO: This actually may return more bits than k, the caller is responsible for masking the
///   result. Better design may be to pass std::span<uint64_t> without specifying k.
template <typename UIntT>
UIntT modinv_pow2(const UIntT& x, unsigned k) noexcept
{
    assert(bit_test(x, 0));        // x must be odd for the inverse to exist.
    assert(k <= UIntT::num_bits);  // k must fit into the type.

    // Start with inversion mod 2⁶⁴.
    UIntT inv = evmmax::modinv(x[0]);

    // Each iteration doubles the number of correct bits in the inverse. See modinv(uint32_t).
    for (size_t iterations = 64; iterations < k; iterations *= 2)
        inv *= 2 - x * inv;

    return inv;
}

/// Computes modular exponentiation for even modulus: base^exp % (mod_odd * 2^k).
template <typename UIntT>
UIntT modexp_even(const UIntT& base, Exponent exp, const UIntT& mod_odd, unsigned k) noexcept
{
    // Follow "Montgomery reduction with even modulus" by Çetin Kaya Koç.
    // https://cetinkayakoc.net/docs/j34.pdf
    assert(k != 0);

    const auto x1 = modexp_odd(base, exp, mod_odd);
    const auto x2 = modexp_pow2(base, exp, k);

    const auto mod_odd_inv = modinv_pow2(mod_odd, k);

    const auto mod_pow2_mask = (UIntT{1} << k) - 1;
    const auto y = ((x2 - x1) * mod_odd_inv) & mod_pow2_mask;
    return x1 + y * mod_odd;
}

template <size_t Size>
void modexp_impl(std::span<const uint8_t> base_bytes, Exponent exp,
    std::span<const uint8_t> mod_bytes, uint8_t* output) noexcept
{
    using UIntT = intx::uint<Size * 8>;
    const auto base = intx::be::load<UIntT>(base_bytes);
    const auto mod = intx::be::load<UIntT>(mod_bytes);
    assert(mod != 0);  // Modulus of zero must be handled outside.

    UIntT result;
    if (exp.bit_width() == 0)                                   // Exponent is 0:
        result = mod != 1;                                      // - result is 1 except mod 1
    else if (const auto mod_tz = ctz(mod); mod_tz == 0)         // Modulus is:
        result = modexp_odd(base, exp, mod);                    // - odd
    else if (const auto mod_odd = mod >> mod_tz; mod_odd == 1)  //
        result = modexp_pow2(base, exp, mod_tz);                // - power of 2
    else                                                        //
        result = modexp_even(base, exp, mod_odd, mod_tz);       // - even

    intx::be::trunc(std::span{output, mod_bytes.size()}, result);
}
}  // namespace

namespace evmone::crypto
{
void modexp(std::span<const uint8_t> base, std::span<const uint8_t> exp,
    std::span<const uint8_t> mod, uint8_t* output) noexcept
{
    static constexpr auto MAX_INPUT_SIZE = 1024;
    assert(base.size() <= MAX_INPUT_SIZE);
    assert(mod.size() <= MAX_INPUT_SIZE);

    const Exponent exp_obj{exp};

    if (const auto size = std::max(mod.size(), base.size()); size <= 16)
        modexp_impl<16>(base, exp_obj, mod, output);
    else if (size <= 32)
        modexp_impl<32>(base, exp_obj, mod, output);
    else if (size <= 64)
        modexp_impl<64>(base, exp_obj, mod, output);
    else if (size <= 128)
        modexp_impl<128>(base, exp_obj, mod, output);
    else if (size <= 256)
        modexp_impl<256>(base, exp_obj, mod, output);
    else
        modexp_impl<MAX_INPUT_SIZE>(base, exp_obj, mod, output);
}
}  // namespace evmone::crypto
