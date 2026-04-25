// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "mulmod.hpp"

namespace evmone::crypto
{
void mul_amm_256(std::span<uint64_t, 4> r, std::span<const uint64_t, 4> x,
    std::span<const uint64_t, 4> y, std::span<const uint64_t, 4> mod, uint64_t mod_inv) noexcept
{
    static constexpr size_t N = 4;

    // Local accumulator t[] avoids aliasing penalties when r overlaps x or y.
    std::array<uint64_t, N> t;  // NOLINT(*-pro-type-member-init)
    const auto t_lo = std::span{t}.subspan<0, N - 1>();
    const auto t_hi = std::span{t}.subspan<1>();
    const auto mod_hi = mod.subspan<1>();

    // First iteration: t is uninitialized, so use mul instead of addmul.
    bool t_carry = false;
    {
        const auto c1 = mul(t, x, y[0]);

        const auto m = t[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + t[0])[1];

        const auto c3 = addmul(t_lo, t_hi, mod_hi, m, c2);
        std::tie(t[N - 1], t_carry) = addc(c1, c3);
    }

    // Remaining 3 iterations.
#pragma GCC unroll N - 1
    for (size_t i = 1; i != N; ++i)
    {
        const auto c1 = addmul(t, t, x, y[i]);
        const auto [sum1, d1] = addc(c1, uint64_t{t_carry});

        const auto m = t[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + t[0])[1];

        const auto c3 = addmul(t_lo, t_hi, mod_hi, m, c2);
        const auto [sum2, d2] = addc(sum1, c3);
        t[N - 1] = sum2;
        assert(!(d1 && d2));
        t_carry = d1 || d2;
    }

    if (t_carry)
        sub(t, mod);

    std::ranges::copy(t, r.begin());
}
}  // namespace evmone::crypto
