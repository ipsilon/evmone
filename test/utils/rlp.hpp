// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <evmc/bytes.hpp>
#include <intx/intx.hpp>
#include <test/state/rlp_common.hpp>
#include <cassert>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace evmone::rlp
{
using evmc::bytes;
using evmc::bytes_view;

namespace internal
{
template <uint8_t ShortBase>
inline bytes encode_length(size_t l)
{
    static_assert(ShortBase + SHORT_LENGTH_LIMIT <= std::numeric_limits<uint8_t>::max(),
        "long base must fit uint8_t");
    static constexpr uint8_t LONG_BASE = ShortBase + SHORT_LENGTH_LIMIT;
    assert(l <= 0xffffff);

    if (l <= SHORT_LENGTH_LIMIT)
        return {static_cast<uint8_t>(ShortBase + l)};
    else if (const auto l0 = static_cast<uint8_t>(l); l <= 0xff)
        return {LONG_BASE + 1, l0};
    else if (const auto l1 = static_cast<uint8_t>(l >> 8); l <= 0xffff)
        return {LONG_BASE + 2, l1, l0};
    else
        return {LONG_BASE + 3, static_cast<uint8_t>(l >> 16), l1, l0};
}

inline bytes wrap_list(const bytes& content)
{
    return internal::encode_length<SHORT_LIST_BASE>(content.size()) + content;
}

template <typename InputIterator>
inline bytes encode_container(InputIterator begin, InputIterator end);
}  // namespace internal

inline bytes_view trim(bytes_view b) noexcept
{
    b.remove_prefix(std::min(b.find_first_not_of(uint8_t{0x00}), b.size()));
    return b;
}

template <typename T>
inline decltype(rlp_encode(std::declval<T>())) encode(const T& v)
{
    return rlp_encode(v);
}

inline bytes encode(bytes_view data)
{
    if (data.size() == 1 && data[0] < SHORT_STRING_BASE)
        return {data[0]};

    return internal::encode_length<SHORT_STRING_BASE>(data.size()) += data;  // Op + not available.
}

inline bytes encode(uint64_t x)
{
    uint8_t b[sizeof(x)];
    intx::be::store(b, x);
    return encode(trim({b, sizeof(b)}));
}

inline bytes encode(const intx::uint256& x)
{
    uint8_t b[sizeof(x)];
    intx::be::store(b, x);
    return encode(trim({b, sizeof(b)}));
}

template <typename T>
inline bytes encode(const std::vector<T>& v)
{
    return internal::encode_container(v.begin(), v.end());
}

/// Encodes the fixed-size collection of heterogeneous values as RLP list.
template <typename... Types>
inline bytes encode_tuple(const Types&... elements)
{
    return internal::wrap_list((encode(elements) + ...));
}

/// Encodes a pair of values as RPL list.
template <typename T1, typename T2>
inline bytes encode(const std::pair<T1, T2>& p)
{
    return encode_tuple(p.first, p.second);
}

/// Encodes the container as RLP list.
///
/// @tparam InputIterator  Type of the input iterator.
/// @param  begin          Begin iterator.
/// @param  end            End iterator.
/// @return                Bytes of the RLP list.
template <typename InputIterator>
inline bytes internal::encode_container(InputIterator begin, InputIterator end)
{
    bytes content;
    for (auto it = begin; it != end; ++it)
        content += encode(*it);
    return wrap_list(content);
}
}  // namespace evmone::rlp
