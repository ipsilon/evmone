// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modexp.hpp"
#include <evmmax/evmmax.hpp>
#include <bit>
#include <memory_resource>
#include <ranges>

using namespace intx;

namespace evmone::crypto
{
namespace
{
/// Adds y to x: x[] += y[]. The result is truncated to the size of x. Returns the carry bit.
constexpr bool add(std::span<uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(x.size() >= y.size());

    bool carry = false;
    for (size_t i = 0; i < y.size(); ++i)
        std::tie(x[i], carry) = addc(x[i], y[i], carry);
    for (size_t i = y.size(); carry && i < x.size(); ++i)
        std::tie(x[i], carry) = addc(x[i], uint64_t{0}, carry);
    return carry;
}

/// Subtracts y from x: x[] -= y[]. The result is truncated to the size of x.
constexpr void sub(std::span<uint64_t> x, std::span<const uint64_t> y) noexcept
{
    assert(x.size() >= y.size());

    bool borrow = false;
    for (size_t i = 0; i < y.size(); ++i)
        std::tie(x[i], borrow) = subc(x[i], y[i], borrow);
    for (size_t i = y.size(); borrow && i < x.size(); ++i)
        std::tie(x[i], borrow) = subc(x[i], uint64_t{0}, borrow);
}

/// Multiplies multi-word x by single word y: r[] = x[] * y. Returns the carry word.
constexpr uint64_t mul(std::span<uint64_t> r, std::span<const uint64_t> x, uint64_t y) noexcept
{
    assert(r.size() == x.size());

    uint64_t c = 0;
#pragma GCC unroll 4
    for (size_t i = 0; i != x.size(); ++i)
    {
        const auto p = umul(x[i], y) + c;
        r[i] = p[0];
        c = p[1];
    }
    return c;
}

/// Multiplies each word of x by y and adds the matching word of p, propagating a carry to the next
/// word. Starts with initial carry c. Stores the result in r. Returns the final carry.
/// r[] = p[] + x[] * y (+ c).
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
    assert(r.size() <= x.size() + y.size());  // No support for zeroing r tail.

    // Ensure y is the shorter one to simplify the implementation and to have shorter outer loop.
    if (x.size() < y.size())
        std::swap(x, y);

    // First iteration: use mul (not addmul) since r is uninitialized.
    const auto hi0 = mul(r.first(x.size()), x, y[0]);
    if (r.size() > x.size())
        r[x.size()] = hi0;

    // Growing phase: each iteration produces a new high word at r[j + x.size()].
    const auto hi_iters = std::min(y.size(), r.size() - x.size());
    for (size_t j = 1; j < hi_iters; ++j)
        r[j + x.size()] = addmul(r.subspan(j, x.size()), r.subspan(j, x.size()), x, y[j]);

    // Truncating phase: product is wider than r, discard high words.
    for (size_t j = std::max(hi_iters, size_t{1}); j < y.size(); ++j)
        addmul(r.subspan(j), r.subspan(j), x.first(r.size() - j), y[j]);
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
    const auto n = x.size();
    assert(r.size() >= n);
    assert(k < n * 64);
    const auto word_shift = k / 64;
    const auto bit_shift = k % 64;

    // Shift words. std::copy requires d_first ∉ [first, last).
    // When word_shift == 0 and r aliases x, it would be a self-copy — skip it.
    if (word_shift != 0 || r.data() != x.data())
        std::ranges::copy(x.subspan(word_shift), r.begin());
    std::ranges::fill(r.subspan(n - word_shift), uint64_t{0});

    // Shift remaining bits in place.
    if (bit_shift != 0)
    {
        for (size_t i = 0; i < n - word_shift - 1; ++i)
            r[i] = (r[i] >> bit_shift) | (r[i + 1] << (64 - bit_shift));
        r[n - word_shift - 1] >>= bit_shift;
    }

    return trim(r.first(n));
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
/// Scratch space required: 2 * u.size() + 2 words.
void rem(std::span<uint64_t> r, std::span<const uint64_t> u, std::span<const uint64_t> d,
    std::span<uint64_t> scratch) noexcept
{
    assert(!d.empty());
    assert(!u.empty());
    assert(d.back() != 0);
    assert(u.back() != 0);
    assert(r.size() >= d.size());
    assert(u.size() > d.size());  // Because used only for to-Montgomery conversion.
    assert(scratch.size() >= 2 * u.size() + 2);

    // Layout: un[u.size()+1] | dn[d.size()] | q[u.size()+1-d.size()]
    auto un = scratch.subspan(0, u.size() + 1);
    const auto dn = scratch.subspan(u.size() + 1, d.size());
    const auto q_buf = scratch.subspan(u.size() + 1 + d.size(), u.size() + 1 - d.size());

    un.back() = 0;  // Only the extra top word needs zeroing; the rest is set by normalization.

    // Normalize: left-shift both u and d so that the MSB of d's top word is set.
    const auto shift = static_cast<unsigned>(std::countl_zero(d.back()));

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
        // General case: Knuth's algorithm. The quotient is stored in q_buf;
        // we don't use it, but udivrem_knuth requires storage for it.
        intx::internal::udivrem_knuth(q_buf.data(), un, dn);
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
///
/// Computes r = x * y * R^-1 mod m (Almost Montgomery Multiplication).
/// r must not alias x or y.
void mul_amm(std::span<uint64_t> r, std::span<const uint64_t> x, std::span<const uint64_t> y,
    std::span<const uint64_t> mod, uint64_t mod_inv) noexcept
{
    // Use Coarsely Integrated Operand Scanning (CIOS) method with the "almost" reduction.
    const auto n = r.size();
    assert(n > 0);
    assert(x.size() == n);
    assert(y.size() == n);
    assert(mod.size() == n);
    assert(mod.back() != 0);
    assert(r.data() != x.data() && r.data() != y.data());  // r must not alias inputs.

    const auto r_lo = r.subspan(0, n - 1);
    const auto r_hi = r.subspan(1);
    const auto mod_hi = mod.subspan(1);

    // First iteration: r is uninitialized, so use mul instead of addmul.
    bool r_carry = false;
    {
        const auto c1 = mul(r, x, y[0]);

        const auto m = r[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + r[0])[1];

        const auto c3 = addmul(r_lo, r_hi, mod_hi, m, c2);
        std::tie(r[n - 1], r_carry) = intx::addc(c1, c3);
    }

    // Remaining iterations.
    for (size_t i = 1; i != n; ++i)
    {
        const auto c1 = addmul(r, r, x, y[i]);
        const auto [sum1, d1] = intx::addc(c1, uint64_t{r_carry});

        const auto m = r[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + r[0])[1];

        const auto c3 = addmul(r_lo, r_hi, mod_hi, m, c2);
        const auto [sum2, d2] = intx::addc(sum1, c3);
        r[n - 1] = sum2;
        assert(!(d1 && d2));
        r_carry = d1 || d2;
    }

    if (r_carry)
        sub(r, mod);
}

/// Computes result[] = base[]^exp % mod[] for odd mod[] (mod[0] % 2 != 0).
/// Scratch space required: 4n + 3*base.size() + 2 words, where n = mod.size().
void modexp_odd(std::span<uint64_t> result, std::span<const uint64_t> base, Exponent exp,
    std::span<const uint64_t> mod, std::span<uint64_t> scratch) noexcept
{
    assert(!mod.empty() && mod.back() != 0);    // mod must be trimmed.
    assert(!base.empty() && base.back() != 0);  // base must be trimmed.
    assert(result.size() == mod.size());
    assert(exp.bit_width() != 0);

    const auto n = mod.size();
    const auto mod_inv = -evmmax::modinv(mod[0]);

    // Layout: u[n+base.size()] | base_mont[n] | t/rem_scratch[max(n, 2*(n+base.size())+2)]
    // t and rem_scratch share the same region (exclusive lifetimes).
    assert(scratch.size() >= 4 * n + 3 * base.size() + 2);

    // Compute base_mont = (base * R) % mod, where R = 2^(n*64).
    // The numerator u = base << (n*64): base in the upper words, lower n words are zero.
    const auto u = scratch.subspan(0, n + base.size());
    const auto base_mont = scratch.subspan(n + base.size(), n);
    const auto rem_scratch = scratch.subspan(2 * n + base.size(), 2 * n + 2 * base.size() + 2);

    std::ranges::fill(u.first(n), uint64_t{0});  // Lower n words of u must be zero.
    std::ranges::copy(base, u.subspan(n).begin());
    rem(base_mont, u, mod, rem_scratch);

    // Double-buffer: r1 always holds the current value, r2 is scratch for mul_amm output.
    auto r_cur = result;
    auto r_tmp = u.first(n);  // Reuse u scratch space.
    std::ranges::copy(base_mont, r_cur.begin());

    for (auto i = exp.bit_width() - 1; i != 0; --i)
    {
        mul_amm(r_tmp, r_cur, r_cur, mod, mod_inv);  // Square: r2 = r1 * r1.
        if (exp[i - 1])
            mul_amm(r_cur, r_tmp, base_mont, mod, mod_inv);  // Multiply: r1 = r2 * base_mont.
        else
            std::swap(r_cur, r_tmp);  // No multiply: adopt r2 as r1.
    }

    // Convert from Montgomery form: multiply by 1.
    std::ranges::fill(base_mont, uint64_t{0});
    base_mont[0] = 1;
    mul_amm(r_tmp, r_cur, base_mont, mod, mod_inv);
    std::swap(r_cur, r_tmp);

    // Reduce if necessary: AMM can produce mod <= r < 2*mod.
    if (!less(r_cur, mod))
        sub(r_cur, mod);
    assert(less(r_cur, mod));

    // If the result ended up in the scratch buffer, copy to result.
    if (r_cur.data() != result.data())
        std::ranges::copy(r_cur, result.begin());
}

/// Trims the multi-word number x[] to k bits.
void mask_pow2(std::span<uint64_t> x, unsigned k) noexcept
{
    assert(k != 0);
    assert(x.size() == (k + 63) / 64);
    // This implementation assumes the x.size() matches the k so we always mask the top word.
    // For k % 64 == 0, we don't mask anything.
    x.back() &= ~uint64_t{0} >> (-k % 64);
}

/// Computes r[] = base[]^exp % 2^k.
/// Only the low-order words matching the k bits of the base are used.
/// Also, the same amount of the result words are produced. The rest is not modified.
/// Scratch space required: (k + 63) / 64 words.
void modexp_pow2(std::span<uint64_t> r, std::span<const uint64_t> base, Exponent exp, unsigned k,
    std::span<uint64_t> scratch) noexcept
{
    assert(k != 0);                             // Modulus of 1 should be covered as "odd".
    assert(exp.bit_width() != 0);               // Exponent of zero must be handled outside.
    assert(!base.empty() && base.back() != 0);  // base must be trimmed.
    assert(r.data() != base.data());            // No in-place operation.

    const size_t num_pow2_words = (k + 63) / 64;
    assert(r.size() >= num_pow2_words);
    assert(scratch.size() >= num_pow2_words);

    auto r_k = r.subspan(0, num_pow2_words);
    auto tmp = scratch.subspan(0, num_pow2_words);

    const auto base_k = base.subspan(0, std::min(base.size(), num_pow2_words));

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

    // r_k may point to scratch. Copy back to the result buffer if needed.
    if (r_k.data() != r.data())
        std::ranges::copy(r_k, r.begin());
}

/// Computes modular inversion of the multi-word number x[] modulo 2^(r.size() * 64).
/// Scratch space required: 2 * r.size() words.
void modinv_pow2(
    std::span<uint64_t> r, std::span<const uint64_t> x, std::span<uint64_t> scratch) noexcept
{
    assert(!x.empty() && (x[0] & 1) != 0);  // x must be odd.
    assert(!r.empty());
    assert(scratch.size() >= 2 * r.size());

    r[0] = evmmax::modinv(x[0]);                   // Good start: 64 correct bits.
    std::ranges::fill(r.subspan(1), uint64_t{0});  // Zero the rest for correct final subtraction.

    // Newton-Raphson iteration for modular inverse: inv' = inv * (2 - x * inv).
    // Rearranged as: inv' = 2 * inv - x * inv^2, which avoids the (2 - x) negation helper
    // and computes the result directly into r (no copy needed).
    // Each iteration doubles the number of correct bits. See evmmax::modinv().
    for (size_t i = 1; i < r.size(); i *= 2)
    {
        // We have i-word correct inverse in r[0..i). Double the precision to n = min(2i, r.size()).
        const auto n = std::min(i * 2, r.size());
        assert(n > i);
        const auto t1 = scratch.subspan(0, n);
        const auto t2 = scratch.subspan(n, n);

        const auto inv = r.first(i);
        mul(t1, inv, inv);                            // t1 = inv^2
        mul(t2, t1, x.first(std::min(n, x.size())));  // t2 = inv^2 * x, x clamped to n words.
        r[i] = uint64_t{add(inv, inv)};               // r[0..i+1) = 2 * inv, carry into r[i]
        sub(r.first(n), t2);                          // r[0..n) = 2 * inv - x * inv^2
    }
}

}  // namespace

void modexp(std::span<const uint8_t> base_bytes, std::span<const uint8_t> exp_bytes,
    std::span<const uint8_t> mod_bytes, uint8_t* output) noexcept
{
    const Exponent exp{exp_bytes};

    const auto base_size = (base_bytes.size() + 7) / 8;
    const auto mod_size = (mod_bytes.size() + 7) / 8;

    // Bump allocator for all working memory (values + scratch).
    // Stack buffer covers inputs up to the EIP-7823 limit (1024 bytes).
    // Capacity: values[b+2m] + op scratch[4m+3b+2] + CRT[m+2] = 4b+7m+4 words.
    // The worst case is an even modulus with 1 trailing zero bit (odd_size=m, pow2_size=1).
    static constexpr size_t MAX_SIZE = 1024 / sizeof(uint64_t);  // EIP-7823
    static constexpr size_t STACK_CAPACITY = 4 * MAX_SIZE + 7 * MAX_SIZE + 4;
    alignas(uint64_t) std::byte stack_buf[STACK_CAPACITY * sizeof(uint64_t)];
    std::pmr::monotonic_buffer_resource pool{stack_buf, sizeof(stack_buf)};
    std::pmr::polymorphic_allocator<uint64_t> alloc{&pool};

    // Allocate and load values.
    const auto base = load({alloc.allocate(base_size), base_size}, base_bytes);
    const auto [mod_odd, mod_tz] = load_mod({alloc.allocate(mod_size), mod_size}, mod_bytes);
    assert(!mod_odd.empty());  // Modulus of zero must be handled outside.
    const auto result = std::span{alloc.allocate(mod_size), mod_size};
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
    else
    {
        // The main part is approached by following the procedure for the most general case of an
        // even modulus, conditionally skipping trivial sub-parts.
        // See "Montgomery reduction with even modulus" by Çetin Kaya Koç.
        // https://cetinkayakoc.net/docs/j34.pdf

        // The "odd" part is trivial if the modulus is a pure power of two.
        const auto odd_is_trivial = mod_odd.size() == 1 && mod_odd[0] == 1;

        // The "power-of-two" part is trivial if the modulus is odd.
        const auto pow2_is_trivial = mod_tz == 0;

        const auto odd_size = mod_odd.size();
        const size_t pow2_size = (mod_tz + 63) / 64;

        // Combining results via CRT is needed when both parts are non-trivial.
        const auto need_crt = !pow2_is_trivial && !odd_is_trivial;

        // Allocate operation scratch (dead after each call, reused sequentially).
        const size_t odd_scratch = !odd_is_trivial ? 4 * odd_size + 3 * base.size() + 2 : 0;
        const size_t pow2_scratch = !pow2_is_trivial ? pow2_size : 0;
        const size_t inv_scratch = need_crt ? 2 * pow2_size : 0;
        const size_t op_scratch_size = std::max({odd_scratch, pow2_scratch, inv_scratch});
        const auto op_scratch = std::span{alloc.allocate(op_scratch_size), op_scratch_size};

        // Place the odd result directly in the result buffer if the CRT is not needed.
        const auto result_odd =
            need_crt ? std::span{alloc.allocate(odd_size), odd_size} : result.first(odd_size);
        // Always place the power-of-two result in the result buffer.
        const auto result_pow2 = result.first(pow2_size);

        if (!odd_is_trivial) [[likely]]
            modexp_odd(result_odd, base, exp, mod_odd, op_scratch);

        if (!pow2_is_trivial)
            modexp_pow2(result_pow2, base, exp, mod_tz, op_scratch);

        if (need_crt)
        {
            const auto mod_odd_inv = std::span{alloc.allocate(pow2_size), pow2_size};
            const auto y = std::span{alloc.allocate(pow2_size), pow2_size};

            modinv_pow2(mod_odd_inv, mod_odd, op_scratch);
            sub(result_pow2, result_odd.first(std::min(odd_size, pow2_size)));
            mul(y, result_pow2, mod_odd_inv);
            mask_pow2(y, mod_tz);
            mul(result, mod_odd, y);
            add(result, result_odd);
        }
    }

    store(std::span{output, mod_bytes.size()}, result);
}
}  // namespace evmone::crypto
