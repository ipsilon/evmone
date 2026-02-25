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
    assert(r.size() == std::max(x.size(), y.size()));

    // Ensure y is the shorter one to simplify the implementation and to have shorter outer loop.
    if (x.size() < y.size())
        std::swap(x, y);

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

/// Loads big-endian bytes into little-endian uint64 words.
void load(std::span<uint64_t> r, std::span<const uint8_t> data) noexcept
{
    const auto r_bytes = std::as_writable_bytes(r);
    assert(r_bytes.size() >= data.size());
    const auto padding = r_bytes.size() - data.size();

    // Copy data right-aligned in the output buffer, zero-fill the leading padding.
    const auto after_padding = std::ranges::fill(r_bytes.subspan(0, padding), std::byte{0});
    std::ranges::copy(std::as_bytes(data), after_padding);

    // Convert from big-endian byte layout to little-endian words:
    // reverse word order and byte-swap each word.
    std::ranges::reverse(r);
    for (auto& w : r)
        w = bswap(w);
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

/// Counts trailing zeros in a non-zero little-endian word array.
constexpr unsigned ctz(std::span<const uint64_t> x) noexcept
{
    assert(std::ranges::any_of(x, [](auto w) { return w != 0; }));
    const auto it = std::ranges::find_if(x, [](auto w) { return w != 0; });
    return static_cast<unsigned>((it - x.begin()) * 64 + std::countr_zero(*it));
}

/// Checks if a non-zero multi-word number is a power of two.
constexpr bool is_pow2(std::span<const uint64_t> x) noexcept
{
    assert(std::ranges::any_of(x, [](auto w) { return w != 0; }));
    const auto it = std::ranges::find_if(x, [](auto w) { return w != 0; });
    return std::has_single_bit(*it) &&
           std::ranges::none_of(it + 1, x.end(), [](auto w) { return w != 0; });
}

/// Right-shifts a little-endian word array by k bits.
void shr(std::span<uint64_t> r, std::span<const uint64_t> x, unsigned k) noexcept
{
    const size_t n = x.size();
    assert(r.size() == n);
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
template <size_t N>
constexpr void mul_amm(std::span<uint64_t, N> r, std::span<const uint64_t, N> y,
    std::span<const uint64_t, N> mod, uint64_t mod_inv) noexcept
{
    static_assert(N != std::dynamic_extent);
    // Use Coarsely Integrated Operand Scanning (CIOS) method with the "almost" reduction.

    std::array<uint64_t, N> t_storage{};
    const std::span t{t_storage};
    bool t_carry = false;
    for (size_t i = 0; i != N; ++i)
    {
        const auto c1 = addmul(t, t, r, y[i]);
        const auto [sum1, d1] = intx::addc(c1, t_carry);

        const auto m = t[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + t[0])[1];

        const auto c3 = addmul(t.template subspan<0, N - 1>(), t.template subspan<1>(),
            mod.template subspan<1>(), m, c2);
        const auto [sum2, d2] = intx::addc(sum1, c3);
        t[N - 1] = sum2;
        assert(!(d1 && d2));  // At most one carry should be set.
        t_carry = d1 || d2;
    }

    if (t_carry)  // Reduce if t >= R.
        sub(t, mod);

    std::ranges::copy(t, r.begin());
}

template <typename UIntT>
UIntT modexp_odd_fixed_size(const UIntT& base, Exponent exp, const UIntT& mod) noexcept
{
    static constexpr auto N = UIntT::num_words;
    assert(exp.bit_width() != 0);  // Exponent of zero must be handled outside.

    const auto mod_inv = evmmax::compute_mont_mod_inv(mod);

    /// Convert the base to Montgomery form: base*R % mod, where R = 2^(num_bits).
    const auto base_mont =
        udivrem(intx::uint<UIntT::num_bits * 2>{base} << UIntT::num_bits, mod).rem;

    auto ret = base_mont;
    for (auto i = exp.bit_width() - 1; i != 0; --i)
    {
        mul_amm<N>(as_words(ret), as_words(ret), as_words(mod), mod_inv);
        if (exp[i - 1])
            mul_amm<N>(as_words(ret), as_words(base_mont), as_words(mod), mod_inv);
    }

    // Convert the result from Montgomery form by multiplying with the standard integer 1.
    static constexpr UIntT ONE = 1;
    mul_amm<N>(as_words(ret), as_words(ONE), as_words(mod), mod_inv);

    // Reduce if necessary: AMM can produce mod <= ret < 2*mod.
    if (ret >= mod)
        ret -= mod;
    assert(ret < mod);  // One reduction should be enough.

    return ret;
}

void modexp_odd(std::span<uint64_t> result, const std::span<const uint64_t> base, Exponent exp,
    const std::span<const uint64_t> mod) noexcept
{
    static constexpr auto MAX_INPUT_SIZE = 1024 / sizeof(uint64_t);  // 8192 bits, as in EIP-7823.
    assert(base.size() <= MAX_INPUT_SIZE);
    assert(base.size() <= MAX_INPUT_SIZE);
    assert(result.size() == mod.size());
    assert(base.size() == mod.size());  // True for the current callers. Relax if needed.

    const auto impl = [=]<size_t N>() {
        using UintT = intx::uint<N * 64>;
        const auto r = modexp_odd_fixed_size(UintT{base}, exp, UintT{mod});
        std::ranges::copy(as_words(r).subspan(0, result.size()), result.begin());
    };

    if (const auto n = mod.size(); n <= 2)
        impl.operator()<2>();
    else if (n <= 4)
        impl.operator()<4>();
    else if (n <= 8)
        impl.operator()<8>();
    else if (n <= 16)
        impl.operator()<16>();
    else if (n <= 32)
        impl.operator()<32>();
    else
        impl.operator()<MAX_INPUT_SIZE>();
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
    assert(k != 0);                   // Modulus of 1 should be covered as "odd".
    assert(exp.bit_width() != 0);     // Exponent of zero must be handled outside.
    assert(r.data() != base.data());  // No in-place operation.

    const auto num_pow2_words = (k + 63) / 64;
    assert(r.size() >= num_pow2_words);
    assert(base.size() >= num_pow2_words);

    const auto base_k = base.subspan(0, num_pow2_words);
    auto r_k = r.subspan(0, num_pow2_words);

    // Allocate temporary storage for iterations.
    // TODO: Move to stack if the size is small enough or provide from the caller.
    const auto tmp_storage = std::make_unique_for_overwrite<uint64_t[]>(num_pow2_words);
    auto tmp = std::span{tmp_storage.get(), num_pow2_words};

    std::ranges::copy(base_k, r_k.begin());

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
void modexp_even(std::span<uint64_t> r, const std::span<const uint64_t> base, Exponent exp,
    std::span<const uint64_t> mod_odd, unsigned k)
{
    // Follow "Montgomery reduction with even modulus" by Çetin Kaya Koç.
    // https://cetinkayakoc.net/docs/j34.pdf
    assert(k != 0);
    assert(r.size() == mod_odd.size());

    const auto num_pow2_words = (k + 63) / 64;
    const auto tmp_storage =
        std::make_unique_for_overwrite<uint64_t[]>(mod_odd.size() + num_pow2_words * 2);
    const auto tmp = std::span{tmp_storage.get(), mod_odd.size() + num_pow2_words * 2};
    const auto tmp1 = tmp.subspan(0, mod_odd.size());
    const auto tmp2 = tmp.subspan(mod_odd.size(), num_pow2_words);
    const auto tmp3 = tmp.subspan(mod_odd.size() + num_pow2_words, num_pow2_words);

    const auto x1 = tmp1;
    modexp_odd(x1, base, exp, mod_odd);

    const auto x2 = r.subspan(0, num_pow2_words);  // Reuse the result storage.
    modexp_pow2(x2, base, exp, k);

    const auto mod_odd_inv = tmp2;
    modinv_pow2(mod_odd_inv, mod_odd);

    const auto y = tmp3;
    sub(x2, std::span(x1).subspan(0, num_pow2_words));
    mul(y, x2, mod_odd_inv);
    mask_pow2(y, k);
    mul(r, y, mod_odd);
    add(r, x1);
}
}  // namespace

namespace evmone::crypto
{
void modexp(std::span<const uint8_t> base_bytes, std::span<const uint8_t> exp_bytes,
    std::span<const uint8_t> mod_bytes, uint8_t* output) noexcept
{
    const Exponent exp{exp_bytes};

    const auto w = (std::max(mod_bytes.size(), base_bytes.size()) + 7) / 8;
    const auto storage = std::make_unique_for_overwrite<uint64_t[]>(w * 4);
    const auto base = std::span{storage.get(), w};
    load(base, base_bytes);
    const auto mod = std::span{storage.get() + w, w};
    load(mod, mod_bytes);
    assert(std::ranges::any_of(mod, [](auto x) { return x != 0; }));  // Modulus of zero must be
                                                                      // handled outside.
    const auto result = std::span{storage.get() + w * 2, w};
    std::ranges::fill(result, uint64_t{0});

    if (exp.bit_width() == 0)  // Exponent is 0:
    {
        // Result is 1 except when mod is 1.
        if (mod[0] != 1 || std::ranges::any_of(mod.subspan(1), [](auto x) { return x != 0; }))
            result[0] = 1;
    }
    else if (const auto mod_tz = ctz(mod); mod_tz == 0)  // - odd
    {
        modexp_odd(result, base, exp, mod);
    }
    else if (is_pow2(mod))  // - power of 2
    {
        const auto n = (mod_tz + 63) / 64;
        modexp_pow2(std::span(result).subspan(0, n), std::span{base}.subspan(0, n), exp, mod_tz);
    }
    else  // - even
    {
        const auto mod_odd = std::span{storage.get() + w * 3, w};
        shr(mod_odd, mod, mod_tz);
        modexp_even(result, base, exp, mod_odd, mod_tz);
    }

    store(std::span{output, mod_bytes.size()}, result);
}
}  // namespace evmone::crypto
