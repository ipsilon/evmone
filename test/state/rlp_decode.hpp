// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "rlp_common.hpp"
#include <evmc/evmc.hpp>
#include <intx/intx.hpp>
#include <cassert>
#include <concepts>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

/// Generic RLP decoding primitives (the counterpart of the encoding in test/utils/rlp.hpp).
/// They live in the state library because the transaction decoder is their only user and the
/// state library must not depend on the test-utils library. The primitives never throw: each
/// decode returns false on malformed input.
namespace evmone::rlp
{
using evmc::bytes;
using evmc::bytes_view;

template <class T>
concept UnsignedIntegral =
    std::unsigned_integral<T> || std::same_as<T, intx::uint128> || std::same_as<T, intx::uint256> ||
    std::same_as<T, intx::uint512> || std::same_as<T, intx::uint<2048>>;

/// Loads an unsigned integer from big-endian bytes. The caller must ensure the input is not larger
/// than the destination type.
template <UnsignedIntegral T>
[[nodiscard]] inline T load(bytes_view input) noexcept
{
    assert(input.size() <= sizeof(T));
    T x{};
    std::memcpy(&intx::as_bytes(x)[sizeof(T) - input.size()], input.data(), input.size());
    return intx::to_big_endian(x);
}

/// A decoded RLP item header: the payload length and whether the item is a list.
struct Header
{
    uint64_t payload_length = 0;
    bool is_list = false;
};

/// Decodes the RLP header at the front of @p input into @p out, advancing @p input past the header
/// bytes. Enforces canonical length encoding. Returns false on malformed input.
[[nodiscard]] bool decode_header(bytes_view& input, Header& out) noexcept;

template <UnsignedIntegral T>
[[nodiscard]] bool decode(bytes_view& from, T& to) noexcept
{
    Header h;
    if (!decode_header(from, h) || h.is_list || sizeof(T) < h.payload_length)
        return false;

    // Reject non-canonical integer encoding: no leading zeros allowed.
    if (h.payload_length > 0 && from[0] == 0)
        return false;

    to = load<T>(from.substr(0, static_cast<size_t>(h.payload_length)));
    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

[[nodiscard]] bool decode(bytes_view& from, bytes& to) noexcept;
[[nodiscard]] bool decode(bytes_view& from, evmc::bytes32& to) noexcept;
[[nodiscard]] bool decode(bytes_view& from, evmc::address& to) noexcept;

template <size_t N>
[[nodiscard]] bool decode(bytes_view& from, std::span<uint8_t, N> to) noexcept
{
    Header h;
    if (!decode_header(from, h) || h.is_list || to.size() < h.payload_length)
        return false;

    const auto d = to.size() - h.payload_length;
    std::memcpy(to.data() + d, from.data(), static_cast<size_t>(h.payload_length));
    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

template <size_t N>
[[nodiscard]] bool decode(bytes_view& from, uint8_t (&to)[N]) noexcept
{
    return decode(from, std::span<uint8_t, N>(to));
}

// Forward declaration so the vector decoder below can decode a vector of pairs.
template <typename T1, typename T2>
[[nodiscard]] bool decode(bytes_view& from, std::pair<T1, T2>& p) noexcept;

template <typename T>
[[nodiscard]] bool decode(bytes_view& from, std::vector<T>& to) noexcept
{
    Header h;
    if (!decode_header(from, h) || !h.is_list || h.payload_length > from.size())
        return false;

    auto payload_view = from.substr(0, static_cast<size_t>(h.payload_length));
    while (!payload_view.empty())
    {
        to.emplace_back();
        if (!decode(payload_view, to.back()))
            return false;
    }

    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

template <typename T1, typename T2>
[[nodiscard]] bool decode(bytes_view& from, std::pair<T1, T2>& p) noexcept
{
    Header h;
    if (!decode_header(from, h) || !h.is_list || h.payload_length > from.size())
        return false;

    auto payload_view = from.substr(0, static_cast<size_t>(h.payload_length));
    if (!decode(payload_view, p.first) || !decode(payload_view, p.second) || !payload_view.empty())
        return false;

    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

/// Decodes a run of fields in order, stopping at the first failure.
template <typename... Ts>
[[nodiscard]] inline bool decode_fields(bytes_view& from, Ts&... items) noexcept
{
    return (decode(from, items) && ...);
}
}  // namespace evmone::rlp
