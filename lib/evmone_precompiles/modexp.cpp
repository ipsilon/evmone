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

template <unsigned N>
constexpr unsigned ctz(const intx::uint<N>& x) noexcept
{
    unsigned tz = 0;
    for (size_t i = 0; i < intx::uint<N>::num_words; ++i)
    {
        if (x[i] != 0)
        {
            tz += static_cast<unsigned>(std::countr_zero(x[i]));
            break;
        }
        tz += intx::uint<N>::word_num_bits;
    }
    return tz;
}

template <typename UIntT>
UIntT modexp_odd(const UIntT& base, std::span<const uint8_t> exp, const UIntT& mod) noexcept
{
    const evmmax::ModArith<UIntT> arith{mod};
    const auto base_mont = arith.to_mont(base);

    auto ret = arith.to_mont(1);
    for (const auto e : exp)
    {
        for (size_t i = 8; i != 0; --i)
        {
            ret = arith.mul(ret, ret);
            const auto bit = (e >> (i - 1)) & 1;
            if (bit != 0)
                ret = arith.mul(ret, base_mont);
        }
    }

    return arith.from_mont(ret);
}

template <typename UIntT>
UIntT modexp_pow2(const UIntT& base, std::span<const uint8_t> exp, unsigned k) noexcept
{
    assert(k != 0);  // Modulus of 1 should be covered as "odd".
    UIntT ret = 1;
    for (auto e : exp)
    {
        for (size_t i = 8; i != 0; --i)
        {
            ret *= ret;
            const auto bit = (e >> (i - 1)) & 1;
            if (bit != 0)
                ret *= base;
        }
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
void modexp_impl(std::span<const uint8_t> base_bytes, std::span<const uint8_t> exp,
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

    const auto it = std::ranges::find_if(exp, [](auto x) { return x != 0; });
    exp = std::span{it, exp.end()};

    if (const auto size = std::max(mod.size(), base.size()); size <= 16)
        modexp_impl<16>(base, exp, mod, output);
    else if (size <= 32)
        modexp_impl<32>(base, exp, mod, output);
    else if (size <= 48)
        modexp_impl<48>(base, exp, mod, output);
    else if (size <= 64)
        modexp_impl<64>(base, exp, mod, output);
    else if (size <= 128)
        modexp_impl<128>(base, exp, mod, output);
    else if (size <= 256)
        modexp_impl<256>(base, exp, mod, output);
    else
        modexp_impl<MAX_INPUT_SIZE>(base, exp, mod, output);
}
}  // namespace evmone::crypto
