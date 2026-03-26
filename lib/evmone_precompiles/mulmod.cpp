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
    const auto r_lo = r.subspan<0, 3>();
    const auto r_hi = r.subspan<1>();
    const auto mod_hi = mod.subspan<1>();

    // First iteration: r is uninitialized, so use mul instead of addmul.
    bool r_carry = false;
    {
        const auto c1 = mul(r, x, y[0]);

        const auto m = r[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + r[0])[1];

        const auto c3 = addmul(r_lo, r_hi, mod_hi, m, c2);
        std::tie(r[N - 1], r_carry) = addc(c1, c3);
    }

    // Remaining 3 iterations.
#pragma GCC unroll N - 1
    for (size_t i = 1; i != N; ++i)
    {
        const auto c1 = addmul(r, r, x, y[i]);
        const auto [sum1, d1] = addc(c1, uint64_t{r_carry});

        const auto m = r[0] * mod_inv;
        const auto c2 = (umul(mod[0], m) + r[0])[1];

        const auto c3 = addmul(r_lo, r_hi, mod_hi, m, c2);
        const auto [sum2, d2] = addc(sum1, c3);
        r[N - 1] = sum2;
        assert(!(d1 && d2));
        r_carry = d1 || d2;
    }

    if (r_carry)
        sub(r, mod);
}
}  // namespace evmone::crypto
