// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace evmone::crypto
{
// TODO(C++23): Use std::byteswap.
template <std::integral T>
constexpr T byteswap(T value) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
}

/// Converts native representation to/from little-endian.
inline auto to_le(std::integral auto x) noexcept
{
    if constexpr (std::endian::native == std::endian::big)
        return byteswap(x);
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
