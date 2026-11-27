// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>

namespace evmone::crypto
{
/// Converts native representation to/from little-endian.
inline auto to_le(std::integral auto x) noexcept
{
    if constexpr (std::endian::native == std::endian::big)
        return std::byteswap(x);
    return x;
}

template <std::integral T>
inline T load_le(const std::byte* data) noexcept
{
    std::array<std::byte, sizeof(T)> bytes{};
    std::copy_n(data, sizeof(T), bytes.begin());
    return to_le(std::bit_cast<T>(bytes));
}

inline std::byte* store_le(std::byte* out, std::integral auto x) noexcept
{
    return std::ranges::copy(std::bit_cast<std::array<std::byte, sizeof(x)>>(to_le(x)), out).out;
}
}  // namespace evmone::crypto
