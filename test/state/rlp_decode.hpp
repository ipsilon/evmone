// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "rlp_common.hpp"
#include <evmc/evmc.hpp>
#include <intx/intx.hpp>
#include <algorithm>
#include <concepts>
#include <span>
#include <utility>
#include <vector>

/// RLP decoding primitives, the counterpart of the encoding in test/utils/rlp.hpp.
/// Each decode returns false on malformed input; none throw.
namespace evmone::rlp
{
using evmc::bytes;
using evmc::bytes_view;

template <class T>
concept UnsignedIntegral = std::unsigned_integral<T> || std::same_as<T, intx::uint256>;

/// Loads an unsigned integer from big-endian bytes no wider than T.
template <UnsignedIntegral T>
[[nodiscard]] inline T load(bytes_view input) noexcept
{
    return intx::be::load<T>(std::span<const uint8_t>{input.data(), input.size()});
}

struct Header
{
    uint64_t payload_length = 0;
    bool is_list = false;
};

/// Decodes the RLP header, advancing @p input past it. Returns false on malformed input.
/// On success the payload fits the advanced input: out.payload_length <= input.size().
[[nodiscard]] bool decode_header(bytes_view& input, Header& out) noexcept;

template <UnsignedIntegral T>
[[nodiscard]] bool decode(bytes_view& from, T& to) noexcept
{
    Header h;
    if (!decode_header(from, h) || h.is_list || sizeof(T) < h.payload_length)
        return false;

    // Reject leading zeros (non-canonical integer).
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
    // A fixed-width field (address, hash) must be encoded as exactly N bytes.
    Header h;
    if (!decode_header(from, h) || h.is_list || h.payload_length != to.size())
        return false;

    std::ranges::copy(from.substr(0, to.size()), to.begin());
    from.remove_prefix(to.size());
    return true;
}

template <size_t N>
[[nodiscard]] bool decode(bytes_view& from, uint8_t (&to)[N]) noexcept
{
    return decode(from, std::span<uint8_t, N>(to));
}

// Forward declaration for the vector-of-pairs case.
template <typename T1, typename T2>
[[nodiscard]] bool decode(bytes_view& from, std::pair<T1, T2>& p) noexcept;

template <typename T>
[[nodiscard]] bool decode(bytes_view& from, std::vector<T>& to) noexcept
{
    Header h;
    if (!decode_header(from, h) || !h.is_list)
        return false;

    auto payload_view = from.substr(0, static_cast<size_t>(h.payload_length));
    std::vector<T> elements;  // Assigned to `to` only on success, leaving it intact on failure.
    while (!payload_view.empty())
    {
        elements.emplace_back();
        if (!decode(payload_view, elements.back()))
            return false;
    }

    to = std::move(elements);
    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

template <typename T1, typename T2>
[[nodiscard]] bool decode(bytes_view& from, std::pair<T1, T2>& p) noexcept
{
    Header h;
    if (!decode_header(from, h) || !h.is_list)
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
