// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modexp.hpp"
#include <evmmax/evmmax.hpp>
#include <bit>
#include <memory>

using namespace intx;

namespace
{
/// Multiplies each word of x by y and adds the matching word of p, propagating a carry to the next
/// word. Starts with initial carry c. Stores the result in r. Returns the final carry.
/// r[] = p[] + x[] * y (+ c).
/// TODO: Consider [[always_inline]].
/// TODO: Consider template by the span extent.
/// TODO: Consider using pointers for some spans.
constexpr uint64_t addmul(std::span<uint64_t> r, std::span<const uint64_t> p,
    std::span<const uint64_t> x, uint64_t y, uint64_t c = 0) noexcept
{
    assert(r.size() == p.size());
    assert(r.size() == x.size());

#pragma GCC unroll 4
    for (size_t i = 0; i != x.size(); ++i)
    {
        const auto t = umul(x[i], y) + p[i] + c;
        r[i] = t[0];
        c = t[1];
    }
    return c;
}

/// Computes multiplication of x times y and truncates the result to the size of r:
/// r[] = x[] * y[].
constexpr void mul(
    std::span<uint64_t> r, std::span<const uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(!x.empty());
    assert(!y.empty());
    assert(x.size() >= y.size());  // Required for safe subspan arithmetic in the loop.
    assert(r.size() == std::max(x.size(), y.size()));

    std::ranges::fill(r, 0);
    for (size_t j = 0; j < y.size(); ++j)
        addmul(r.subspan(j), r.subspan(j), x.subspan(0, x.size() - j), y[j]);
}

/// Computes x[] = 2 - x[].
constexpr void neg_add2(std::span<uint64_t> x) noexcept
{
    assert(!x.empty());
    bool c = false;

    std::tie(x[0], c) = intx::subc(2, x[0]);
    for (auto it = x.begin() + 1; it != x.end(); ++it)
        std::tie(*it, c) = intx::subc(0, *it, c);
}


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

/// Performs the Almost Montgomery Multiplication (AMM).
///
/// The AMM is a relaxed version of the Montgomery multiplication producing a result in Montgomery
/// form which is in range [0, 2⋅mod) in plain form, i.e., it may be larger than the modulus.
/// This allows to skip the final conditional subtraction in most cases, improving performance.
///
/// The inputs are expected to be in Montgomery form.
/// Additionally, passing y=1 converts x from Montgomery form back to plain form,
/// because for x = aR: mul_amm(x, 1) = aR⋅1⋅R⁻¹ % mod = a % mod.
///
/// See "Efficient Software Implementations of Modular Exponentiation":
/// https://eprint.iacr.org/2011/239.pdf
template <typename UintT>
constexpr UintT mul_amm(const UintT& x, const UintT& y, const UintT& mod, uint64_t mod_inv) noexcept
{
    // Use Coarsely Integrated Operand Scanning (CIOS) method with the "almost" reduction.

    constexpr auto S = UintT::num_words;  // TODO(C++23): Make it static

    UintT t_value;
    const auto t = as_words(t_value);
    bool t_carry = false;
    for (size_t i = 0; i != S; ++i)
    {
        const auto c1 = addmul(t, t, as_words(x), y[i]);
        const auto [sum1, d1] = intx::addc(c1, t_carry);

        const auto m = t[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + t[0])[1];

        const auto c3 = addmul(t.template subspan<0, S - 1>(), t.template subspan<1>(),
            as_words(mod).template subspan<1>(), m, c2);
        const auto [sum2, d2] = intx::addc(sum1, c3);
        t[S - 1] = sum2;
        assert(!(d1 && d2));  // At most one carry should be set.
        t_carry = d1 || d2;
    }

    if (t_carry)  // Reduce if t >= R.
        t_value -= mod;

    return t_value;
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
        ret_mont = mul_amm(ret_mont, ret_mont, mod, mod_inv);
        if (exp[i - 1])
            ret_mont = mul_amm(ret_mont, base_mont, mod, mod_inv);
    }

    // Convert the result from Montgomery form by multiplying with the standard integer 1.
    auto ret = mul_amm(ret_mont, UIntT{1}, mod, mod_inv);

    // Reduce if necessary: AMM can produce mod <= ret < 2*mod.
    if (ret >= mod)
        ret -= mod;
    assert(ret < mod);  // One reduction should be enough.

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

/// Computes modular inversion of the multi-word number x[] modulo 2^(r.size() * 64).
void modinv_pow2(std::span<uint64_t> r, std::span<const uint64_t> x) noexcept
{
    assert(!x.empty() && (x[0] & 1) != 0);  // x must be odd.
    assert(r.size() <= x.size());           // Truncating version.
    assert(!r.empty());

    r[0] = evmmax::modinv(x[0]);  // Good start: 64 correct bits.

    // Allocate temporary storage for iterations.
    // TODO: Move to stack if the size is small enough or provide from the caller.
    const auto tmp_storage = std::make_unique_for_overwrite<uint64_t[]>(2 * r.size());
    const auto tmp = std::span{tmp_storage.get(), 2 * r.size()};

    // Each iteration doubles the number of correct bits in the inverse. See evmmax::modinv().
    for (size_t i = 1; i < r.size(); i *= 2)
    {
        // At the start of the iteration we have i-word correct inverse in r[0-i].
        // The iteration performs the Newton-Raphson step with double the precision (n=2i).
        const auto n = std::min(i * 2, r.size());
        const auto t1 = tmp.subspan(0, n);
        const auto t2 = tmp.subspan(n, n);

        mul(t1, x.subspan(0, n), r.subspan(0, i));  // t1 = x * inv
        neg_add2(t1);                               // t1 = 2 - x * inv
        mul(t2, t1, r.subspan(0, i));               // t2 = inv * (2 - x * inv)
        // TODO: Consider implementing the step as (inv << 1) - (x * inv * inv).

        // TODO: Avoid copy by swapping buffers.
        std::ranges::copy(t2, r.begin());
    }
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

    const auto mod_odd_words = as_words(mod_odd);
    UIntT mod_odd_inv;
    const auto num_pow2_words = (k + 63) / 64;
    modinv_pow2(as_words(mod_odd_inv).subspan(0, num_pow2_words), mod_odd_words);

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
