// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modexp.hpp"
#include <evmmax/evmmax.hpp>
#include <bit>

using namespace intx;

namespace
{
template <unsigned N>
void trunc(std::span<uint8_t> dst, const intx::uint<N>& x) noexcept
{
    assert(dst.size() <= N / 8);  // destination must be smaller than the source value
    const auto d = to_big_endian(x);
    std::copy_n(&as_bytes(d)[sizeof(d) - dst.size()], dst.size(), dst.begin());
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

template <typename UIntT>
UIntT modexp_odd(const UIntT& base, Exponent exp, const UIntT& mod) noexcept
{
    const evmmax::ModArith<UIntT> arith{mod};
    const auto base_mont = arith.to_mont(base);

    auto ret = arith.to_mont(1);
    for (auto i = exp.bit_width(); i != 0; --i)
    {
        ret = arith.mul(ret, ret);
        if (exp[i - 1])
            ret = arith.mul(ret, base_mont);
    }

    return arith.from_mont(ret);
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

/// Computes modular inversion for modulus of 2^k.
template <typename UIntT>
UIntT modinv_pow2(const UIntT& x, unsigned k) noexcept
{
    UIntT b = 1;
    UIntT res;
    for (size_t i = 0; i < k; ++i)
    {
        const auto t = b & 1;
        b = (b - x * t) >> 1;
        res |= t << i;
    }
    return res;
}

template <typename UIntT>
UIntT load(std::span<const uint8_t> data) noexcept
{
    static constexpr auto UINT_SIZE = sizeof(UIntT);
    assert(data.size() <= UINT_SIZE);
    uint8_t tmp[UINT_SIZE]{};
    std::ranges::copy(data, &tmp[UINT_SIZE - data.size()]);
    return be::load<UIntT>(tmp);
}

template <size_t Size>
void modexp_impl(std::span<const uint8_t> base_bytes, Exponent exp,
    std::span<const uint8_t> mod_bytes, uint8_t* output) noexcept
{
    using UIntT = intx::uint<Size * 8>;
    const auto base = load<UIntT>(base_bytes);
    const auto mod = load<UIntT>(mod_bytes);

    UIntT result;
    if (const auto mod_tz = ctz(mod); mod_tz == 0)  // is odd
    {
        result = modexp_odd(base, exp, mod);
    }
    else if (const auto mod_odd = mod >> mod_tz; mod_odd == 1)  // is power of 2
    {
        result = modexp_pow2(base, exp, mod_tz);
    }
    else  // is even
    {
        const auto x1 = modexp_odd(base, exp, mod_odd);
        const auto x2 = modexp_pow2(base, exp, mod_tz);

        const auto mod_odd_inv = modinv_pow2(mod_odd, mod_tz);

        const auto mod_pow2_mask = (UIntT{1} << mod_tz) - 1;
        result = x1 + (((x2 - x1) * mod_odd_inv) & mod_pow2_mask) * mod_odd;
    }

    trunc(std::span{output, mod_bytes.size()}, result);
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
