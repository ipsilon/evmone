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
/// The RLP length prefix, left-aligned in a 64-bit word.
struct LengthPrefix
{
    uint64_t word;  ///< The prefix bytes in big-endian order, starting from the top byte.
    unsigned size;  ///< The prefix length in bytes: 1 for the short form, up to 8.
};

/// Computes the RLP length prefix of a payload of @p l bytes.
/// Lengths up to 2^56-1 are supported (up to 7 length bytes).
template <uint8_t ShortBase>
constexpr LengthPrefix length_prefix(size_t l) noexcept
{
    static_assert(ShortBase + SHORT_LENGTH_LIMIT <= std::numeric_limits<uint8_t>::max(),
        "long base must fit uint8_t");
    constexpr uint8_t LONG_BASE = ShortBase + SHORT_LENGTH_LIMIT;
    assert(uint64_t{l} < (uint64_t{1} << 56));

    auto word = uint64_t{ShortBase + l} << 56;
    unsigned n = 0;  // The number of length bytes.
    if (l > SHORT_LENGTH_LIMIT)
    {
        n = (static_cast<unsigned>(std::bit_width(l)) + 7) / 8;
        word = (uint64_t{LONG_BASE + n} << 56) | (uint64_t{l} << (56 - 8 * n));
    }
    return {word, n + 1};
}

/// Encodes the RLP length prefix of the @p payload followed by the payload.
template <uint8_t ShortBase>
inline bytes encode_with_length(bytes_view payload)
{
    const auto prefix = length_prefix<ShortBase>(payload.size());

    // Store the whole prefix word into a 9-byte string and shrink it to the prefix length.
    // The compiler cannot use the assert and only knows prefix.size <= 9 (from bit_width),
    // so the 9-byte size lets it drop the erase() range check.
    // TODO(C++23): Use resize_and_overwrite() and std::byteswap().
    bytes r(sizeof(prefix.word) + 1, 0);
    r.reserve(prefix.size + payload.size());  // Allocate once for the prefix and the payload.
    intx::be::unsafe::store(r.data(), prefix.word);
    r.erase(prefix.size);
    r += payload;
    return r;
}

inline bytes wrap_list(const bytes& content)
{
    return encode_with_length<SHORT_LIST_BASE>(content);
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

    return internal::encode_with_length<SHORT_STRING_BASE>(data);
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
