// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

namespace evmone
{
/// The execution gas counter.
///
/// A strong type wrapping int64_t to prevent accidental mixing of gas with other integers.
/// Conversions from/to int64_t are explicit: Gas{value} and static_cast<int64_t>(gas).
/// The operators take raw int64_t costs, so instruction implementations are unchanged.
/// This is an enum (not a class) to keep the generated code identical to plain int64_t.
enum class Gas : int64_t
{
};

constexpr Gas& operator-=(Gas& gas, int64_t cost) noexcept
{
    return gas = Gas{static_cast<int64_t>(gas) - cost};
}
constexpr Gas& operator+=(Gas& gas, int64_t amount) noexcept
{
    return gas = Gas{static_cast<int64_t>(gas) + amount};
}
constexpr Gas operator-(Gas a, Gas b) noexcept
{
    return Gas{static_cast<int64_t>(a) - static_cast<int64_t>(b)};
}
constexpr Gas operator+(Gas a, int64_t b) noexcept
{
    return Gas{static_cast<int64_t>(a) + b};
}
constexpr Gas operator/(Gas a, int64_t b) noexcept
{
    return Gas{static_cast<int64_t>(a) / b};
}
constexpr bool operator==(Gas a, int64_t b) noexcept
{
    return static_cast<int64_t>(a) == b;
}
constexpr bool operator<(Gas a, int64_t b) noexcept
{
    return static_cast<int64_t>(a) < b;
}
constexpr bool operator<=(Gas a, int64_t b) noexcept
{
    return static_cast<int64_t>(a) <= b;
}
constexpr bool operator>=(Gas a, int64_t b) noexcept
{
    return static_cast<int64_t>(a) >= b;
}
}  // namespace evmone
