// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modexp.hpp"
#include <evmmax/evmmax.hpp>
#include <bit>
#include <memory>
#include <ranges>

using namespace intx;

namespace
{
/// Adds y to x: x[] += y[]. The result is truncated to the size of x.
/// The x and y must be of the same size.
constexpr void add(std::span<uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(x.size() == y.size());

    bool carry = false;
    for (size_t i = 0; i < x.size(); ++i)
        std::tie(x[i], carry) = addc(x[i], y[i], carry);
}

/// Subtracts y from x: x[] -= y[]. The result is truncated to the size of x.
/// The x and y must be of the same size.
constexpr void sub(std::span<uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(x.size() == y.size());

    bool borrow = false;
    for (size_t i = 0; i < x.size(); ++i)
        std::tie(x[i], borrow) = subc(x[i], y[i], borrow);
}

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
    assert(r.size() >= std::max(x.size(), y.size()));

    // Ensure y is the shorter one to simplify the implementation and to have shorter outer loop.
    if (x.size() < y.size())
        std::swap(x, y);

    std::ranges::fill(r, 0);
    for (size_t j = 0; j < y.size(); ++j)
    {
        const auto len = std::min(x.size(), r.size() - j);
        auto c = addmul(r.subspan(j, len), r.subspan(j, len), x.subspan(0, len), y[j]);
        for (auto pos = j + len; pos < r.size() && c != 0; ++pos)
        {
            bool carry;
            std::tie(r[pos], carry) = addc(r[pos], c);
            c = carry ? 1 : 0;
        }
    }
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

/// Trims a little-endian word array to significant words.
template <typename T>
constexpr std::span<T> trim(std::span<T> x) noexcept
{
    const auto it = std::ranges::find_if(x.rbegin(), x.rend(), [](auto w) { return w != 0; });
    return x.first(static_cast<size_t>(std::ranges::distance(it, x.rend())));
}

/// Loads big-endian bytes into little-endian uint64 words.
/// Returns a subspan trimmed to significant (non-zero) words.
std::span<uint64_t> load(std::span<uint64_t> storage, std::span<const uint8_t> data) noexcept
{
    const auto r_bytes = std::as_writable_bytes(storage);
    assert(r_bytes.size() >= data.size());
    const auto padding = r_bytes.size() - data.size();

    // Copy data right-aligned in the output buffer, zero-fill the leading padding.
    const auto after_padding = std::ranges::fill(r_bytes.subspan(0, padding), std::byte{0});
    std::ranges::copy(std::as_bytes(data), after_padding);

    // Convert from big-endian byte layout to little-endian words:
    // reverse word order and byte-swap each word.
    std::ranges::reverse(storage);
    for (auto& w : storage)
        w = bswap(w);

    return trim(storage);
}

/// Stores little-endian uint64 words to big-endian bytes.
void store(std::span<uint8_t> r, std::span<const uint64_t> words) noexcept
{
    // Write full byteswapped words from the end (the least significant word first).
    size_t w = 0;
    auto pos = r.size();
    for (; w < words.size() && pos >= 8; ++w)
    {
        pos -= 8;
        const auto word = bswap(words[w]);
        std::memcpy(&r[pos], &word, 8);
    }

    // Handle remaining partial bytes at the beginning.
    // Assumes little-endian host: after bswap, high-order bytes of the BE value
    // are at the end of the word's memory representation.
    if (w < words.size() && pos > 0)
    {
        const auto word = bswap(words[w]);
        std::memcpy(r.data(), reinterpret_cast<const uint8_t*>(&word) + (8 - pos), pos);
        pos = 0;
    }

    // Zero-fill leading padding.
    std::ranges::fill(r.subspan(0, pos), uint8_t{0});
}

/// Compares two same-size little-endian word arrays as unsigned integers: returns true if x < y.
constexpr bool less(std::span<const uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(x.size() == y.size());
    return std::ranges::lexicographical_compare(std::views::reverse(x), std::views::reverse(y));
}

/// Right-shifts a little-endian word array by k bits.
/// Returns a subspan trimmed to significant (non-zero) words.
std::span<const uint64_t> shr(
    std::span<uint64_t> r, std::span<const uint64_t> x, unsigned k) noexcept
{
    const size_t n = x.size();
    assert(r.size() >= n);
    assert(k < n * 64);
    const auto word_shift = k / 64;
    const auto bit_shift = k % 64;

    // Shift words.
    std::ranges::copy(x.subspan(word_shift), r.begin());
    std::ranges::fill(r.subspan(n - word_shift), uint64_t{0});

    // Shift remaining bits in place.
    if (bit_shift != 0)
    {
        for (size_t i = 0; i < n - word_shift - 1; ++i)
            r[i] = (r[i] >> bit_shift) | (r[i + 1] << (64 - bit_shift));
        r[n - word_shift - 1] >>= bit_shift;
    }

    return trim(std::span<const uint64_t>(r).first(n));
}

/// Result of loading the modulus: the odd part and trailing zero count.
struct ModLoad
{
    std::span<const uint64_t> mod_odd;  ///< Trimmed odd part (shifted in-place).
    unsigned mod_tz;                    ///< Total trailing zero bits (0 = odd modulus).
};

/// Loads modulus from big-endian bytes and extracts the odd part.
/// The odd part is shifted in-place within the storage buffer.
ModLoad load_mod(std::span<uint64_t> storage, std::span<const uint8_t> data) noexcept
{
    const auto top = load(storage, data);
    assert(!top.empty());  // Modulus of zero must be handled outside.

    // Find first non-zero word from bottom.
    const auto it = std::ranges::find_if(top, [](auto w) { return w != 0; });
    // Always found: top is trimmed so top.back() != 0.

    const auto tz_words = static_cast<size_t>(it - top.begin());
    const auto bit_shift = static_cast<unsigned>(std::countr_zero(*it));
    const auto mod_tz = static_cast<unsigned>(tz_words * 64 + bit_shift);

    if (mod_tz == 0)
        return {top, 0};

    // Right-shift in-place to extract the odd part.
    return {shr(storage, top, mod_tz), mod_tz};
}


/// Computes r[] = u[] % d[] (remainder only).
/// The d[] must be non-zero. The r[] size must be >= num significant words in d[].
void rem(std::span<uint64_t> r, std::span<const uint64_t> u, std::span<const uint64_t> d) noexcept
{
    assert(!d.empty());
    assert(!u.empty());
    assert(d.back() != 0);
    assert(u.back() != 0);
    assert(r.size() >= d.size());
    assert(u.size() > d.size());  // Because used only for to-Montgomery conversion.

    const auto un_storage = std::make_unique_for_overwrite<uint64_t[]>(u.size() + 1);
    auto un = std::span{un_storage.get(), u.size() + 1};
    un.back() = 0;  // Only the extra top word needs zeroing; the rest is set by normalization.

    // Normalize: left-shift both u and d so that the MSB of d's top word is set.
    const auto shift = static_cast<unsigned>(std::countl_zero(d.back()));

    // Allocate normalized divisor.
    const auto dn_storage = std::make_unique_for_overwrite<uint64_t[]>(d.size());
    const auto dn = std::span{dn_storage.get(), d.size()};

    if (shift != 0)
    {
        for (size_t i = d.size() - 1; i != 0; --i)
            dn[i] = (d[i] << shift) | (d[i - 1] >> (64 - shift));
        dn[0] = d[0] << shift;

        // Normalize numerator into un.
        un[u.size()] = u.back() >> (64 - shift);
        for (size_t i = u.size() - 1; i != 0; --i)
            un[i] = (u[i] << shift) | (u[i - 1] >> (64 - shift));
        un[0] = u[0] << shift;
    }
    else
    {
        std::ranges::copy(d, dn.begin());
        std::ranges::copy(u, un.begin());
    }

    // Shrink off the extra top word if it is not significant for the normalized numerator.
    if (un.back() == 0 && un[un.size() - 2] < dn.back())
        un = un.first(un.size() - 1);

    const auto denormalize = [&r, shift](std::span<const uint64_t> x) noexcept {
        assert(shift < 64);  // Normalization shift is sub-word.
        assert(r.size() >= x.size());
        shr(r.first(x.size()), x, shift);
        std::ranges::fill(r.subspan(x.size()), uint64_t{0});
    };

    assert(un.size() > dn.size());  // Not possible in the current usage.

    if (dn.size() == 1)
    {
        const uint64_t rem_words[1]{intx::internal::udivrem_by1(un, dn[0])};
        denormalize(rem_words);
    }
    else if (dn.size() == 2)
    {
        const auto rem2 = intx::internal::udivrem_by2(un, uint128{dn[0], dn[1]});
        denormalize(as_words(rem2));
    }
    else
    {
        // General case: Knuth's algorithm. The quotient is stored in the temporary q_storage
        // buffer; we don't use it, but udivrem_knuth requires storage for it.
        const auto q_len = un.size() - dn.size();
        const auto q_storage = std::make_unique_for_overwrite<uint64_t[]>(q_len);
        intx::internal::udivrem_knuth(q_storage.get(), un, dn);
        denormalize(un.subspan(0, dn.size()));
    }
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
void mul_amm(std::span<uint64_t> r, std::span<const uint64_t> y, std::span<const uint64_t> mod,
    uint64_t mod_inv, std::span<uint64_t> t) noexcept
{
    // Use Coarsely Integrated Operand Scanning (CIOS) method with the "almost" reduction.
    const auto n = r.size();
    assert(n > 0);
    assert(y.size() == n);
    assert(mod.size() == n);
    assert(t.size() == n);

    const auto t_lo = t.subspan(0, n - 1);
    const auto t_hi = t.subspan(1);
    const auto mod_hi = mod.subspan(1);

    std::ranges::fill(t, uint64_t{0});
    bool t_carry = false;
    for (size_t i = 0; i != n; ++i)
    {
        const auto c1 = addmul(t, t, r, y[i]);
        const auto [sum1, d1] = intx::addc(c1, uint64_t{t_carry});

        const auto m = t[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + t[0])[1];

        const auto c3 = addmul(t_lo, t_hi, mod_hi, m, c2);
        const auto [sum2, d2] = intx::addc(sum1, c3);
        t[n - 1] = sum2;
        assert(!(d1 && d2));  // At most one carry should be set.
        t_carry = d1 || d2;
    }

    if (t_carry)  // Reduce if t >= R.
        sub(t, mod);

    std::ranges::copy(t, r.begin());
}

void modexp_odd(std::span<uint64_t> result, std::span<const uint64_t> base, Exponent exp,
    std::span<const uint64_t> mod) noexcept
{
    assert(!mod.empty() && mod.back() != 0);    // mod must be trimmed.
    assert(!base.empty() && base.back() != 0);  // base must be trimmed.
    assert(exp.bit_width() != 0);

    const auto n = mod.size();
    const auto mod_inv = -evmmax::modinv(mod[0]);

    // Compute base_mont = (base * R) % mod, where R = 2^(n*64).
    // The numerator u = base << (n*64): base in the upper words, lower n words are zero.
    const auto tmp_storage = std::make_unique_for_overwrite<uint64_t[]>(n + base.size() + n + n);
    const auto u = std::span{tmp_storage.get(), n + base.size()};
    const auto base_mont = std::span{tmp_storage.get() + n + base.size(), n};
    const auto t = std::span{tmp_storage.get() + n + base.size() + n, n};

    std::ranges::fill(u.first(n), uint64_t{0});  // Lower n words of u must be zero.
    std::ranges::copy(base, u.subspan(n).begin());
    rem(base_mont, u, mod);

    // Reuse the lower n words of u as the result buffer r.
    const auto r = u.subspan(0, n);

    std::ranges::copy(base_mont, r.begin());
    for (auto i = exp.bit_width() - 1; i != 0; --i)
    {
        mul_amm(r, r, mod, mod_inv, t);
        if (exp[i - 1])
            mul_amm(r, base_mont, mod, mod_inv, t);
    }

    // Convert the result from Montgomery form by multiplying with 1.
    // Reuse base_mont as ONE (it is no longer needed after the loop).
    std::ranges::fill(base_mont, uint64_t{0});
    base_mont[0] = 1;
    mul_amm(r, base_mont, mod, mod_inv, t);

    // Reduce if necessary: AMM can produce mod <= r < 2*mod.
    if (!less(r, mod))
        sub(r, mod);
    assert(less(r, mod));

    const auto [_, out] = std::ranges::copy(r, result.begin());
    std::ranges::fill(std::span{out, result.end()}, uint64_t{0});
}

/// Trims the multi-word number x[] to k bits.
/// TODO: Currently this assumes no leading zeros in x. Re-design this after modexp is dynamic.
void mask_pow2(std::span<uint64_t> x, unsigned k) noexcept
{
    assert(k != 0);
    assert(x.size() >= (k + 63) / 64);
    assert(!x.empty());
    if (const auto rem = k % 64; rem != 0)
        x.back() &= (uint64_t{1} << rem) - 1;
}

/// Computes r[] = base[]^exp % 2^k.
/// Only the low-order words matching the k bits of the base are used.
/// Also, the same amount of the result words are produced. The rest is not modified.
void modexp_pow2(std::span<uint64_t> r, std::span<const uint64_t> base, Exponent exp, unsigned k)
{
    assert(k != 0);                             // Modulus of 1 should be covered as "odd".
    assert(exp.bit_width() != 0);               // Exponent of zero must be handled outside.
    assert(!base.empty() && base.back() != 0);  // base must be trimmed.
    assert(r.data() != base.data());            // No in-place operation.

    const size_t num_pow2_words = (k + 63) / 64;
    assert(r.size() >= num_pow2_words);

    auto r_k = r.subspan(0, num_pow2_words);

    const auto base_k = base.subspan(0, std::min(base.size(), num_pow2_words));

    // Allocate temporary storage for iterations.
    // TODO: Move to stack if the size is small enough or provide from the caller.
    const auto tmp_storage = std::make_unique_for_overwrite<uint64_t[]>(num_pow2_words);
    auto tmp = std::span{tmp_storage.get(), num_pow2_words};

    const auto [_, pad] = std::ranges::copy(base_k, r_k.begin());
    std::ranges::fill(std::span{pad, r_k.end()}, uint64_t{0});

    for (auto i = exp.bit_width() - 1; i != 0; --i)
    {
        mul(tmp, r_k, r_k);
        std::swap(r_k, tmp);

        if (exp[i - 1])
        {
            mul(tmp, r_k, base_k);
            std::swap(r_k, tmp);
        }
    }

    mask_pow2(r_k, k);

    // r_k may point to the tmp_storage. Copy back to the result buffer if needed.
    if (r_k.data() != r.data())
        std::ranges::copy(r_k, r.begin());
}

/// Computes modular inversion of the multi-word number x[] modulo 2^(r.size() * 64).
void modinv_pow2(std::span<uint64_t> r, std::span<const uint64_t> x) noexcept
{
    assert(!x.empty() && (x[0] & 1) != 0);  // x must be odd.
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

        // Clamp x to available words: high words beyond x.size() are implicitly zero.
        mul(t1, x.subspan(0, std::min(n, x.size())), r.subspan(0, i));  // t1 = x * inv
        neg_add2(t1);                               // t1 = 2 - x * inv
        mul(t2, t1, r.subspan(0, i));               // t2 = inv * (2 - x * inv)
        // TODO: Consider implementing the step as (inv << 1) - (x * inv * inv).

        // TODO: Avoid copy by swapping buffers.
        std::ranges::copy(t2, r.begin());
    }
}

/// Computes modular exponentiation for even modulus: base^exp % (mod_odd * 2^k).
void modexp_even(std::span<uint64_t> r, const std::span<const uint64_t> base, Exponent exp,
    std::span<const uint64_t> mod_odd, unsigned k)
{
    // Follow "Montgomery reduction with even modulus" by Çetin Kaya Koç.
    // https://cetinkayakoc.net/docs/j34.pdf
    assert(k != 0);
    assert(!base.empty() && base.back() != 0);        // base must be trimmed.
    assert(!mod_odd.empty() && mod_odd.back() != 0);  // mod_odd must be trimmed.

    const size_t num_pow2_words = (k + 63) / 64;
    assert(r.size() >= std::max(mod_odd.size(), num_pow2_words));

    const auto tmp_storage =
        std::make_unique_for_overwrite<uint64_t[]>(r.size() + num_pow2_words * 2);
    const auto x1 = std::span{tmp_storage.get(), r.size()};
    const auto mod_odd_inv = std::span{tmp_storage.get() + r.size(), num_pow2_words};
    const auto y = std::span{tmp_storage.get() + r.size() + num_pow2_words, num_pow2_words};

    modinv_pow2(mod_odd_inv, mod_odd);
    modexp_odd(x1, base, exp, mod_odd);

    const auto x2 = r.subspan(0, num_pow2_words);  // Reuse the result storage.
    modexp_pow2(x2, base, exp, k);

    sub(x2, std::span<const uint64_t>(x1).subspan(0, num_pow2_words));
    mul(y, x2, mod_odd_inv);
    mask_pow2(y, k);
    mul(r, mod_odd, y);
    add(r, x1);
}
}  // namespace

namespace evmone::crypto
{
void modexp(std::span<const uint8_t> base_bytes, std::span<const uint8_t> exp_bytes,
    std::span<const uint8_t> mod_bytes, uint8_t* output) noexcept
{
    const Exponent exp{exp_bytes};

    const auto base_size = (base_bytes.size() + 7) / 8;
    const auto mod_size = (mod_bytes.size() + 7) / 8;
    const auto storage = std::make_unique_for_overwrite<uint64_t[]>(base_size + mod_size * 2);
    const auto base = load({storage.get(), base_size}, base_bytes);
    const auto [mod_odd, mod_tz] = load_mod({storage.get() + base_size, mod_size}, mod_bytes);
    assert(!mod_odd.empty());  // Modulus of zero must be handled outside.
    const auto result = std::span{storage.get() + base_size + mod_size, mod_size};
    std::ranges::fill(result, uint64_t{0});

    if (exp.bit_width() == 0)  // Exponent is 0:
    {
        // Result is 1 except when mod is 1.
        if (mod_tz != 0 || mod_odd.size() != 1 || mod_odd[0] != 1)  // mod != 1
            result[0] = 1;
    }
    else if (base.empty())  // base is 0: 0^exp = 0 for exp > 0.
    {
    }
    else if (mod_tz == 0)  // - odd
    {
        modexp_odd(result, base, exp, mod_odd);
    }
    else if (mod_odd.size() == 1 && mod_odd[0] == 1)  // - power of 2
    {
        const auto n = (mod_tz + 63) / 64;
        modexp_pow2(result.subspan(0, n), base, exp, mod_tz);
    }
    else  // - even
    {
        modexp_even(result, base, exp, mod_odd, mod_tz);
    }

    store(std::span{output, mod_bytes.size()}, result);
}
}  // namespace evmone::crypto
