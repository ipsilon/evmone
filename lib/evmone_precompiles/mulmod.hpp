// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <intx/intx.hpp>
#include <span>

namespace evmone::crypto
{
using namespace intx;

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
/// word. Starts with the initial carry c. Stores the result in r. Returns the final carry.
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

/// Almost Montgomery Multiplication for 256-bit (4-word) operands.
/// Computes r = x * y * R^-1 mod m.
/// Arguments x and y must be already in the "almost" Montgomery form.
/// Result r must not alias x or y.
void mul_amm_256(std::span<uint64_t, 4> r, std::span<const uint64_t, 4> x,
    std::span<const uint64_t, 4> y, std::span<const uint64_t, 4> mod, uint64_t mod_inv) noexcept;

}  // namespace evmone::crypto
