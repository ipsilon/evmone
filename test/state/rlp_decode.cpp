// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "rlp_decode.hpp"

namespace evmone::rlp
{
bool decode_header(bytes_view& input, Header& out) noexcept
{
    constexpr uint8_t LONG_STRING_BASE = SHORT_STRING_BASE + SHORT_LENGTH_LIMIT;  // 0xb7
    constexpr uint8_t LONG_LIST_BASE = SHORT_LIST_BASE + SHORT_LENGTH_LIMIT;      // 0xf7

    const auto input_len = input.size();

    if (input_len == 0)
        return false;

    const auto prefix = input[0];

    if (prefix < SHORT_STRING_BASE)  // [0x00, 0x7f] a single byte is its own encoding.
    {
        out = {1, false};
        return true;
    }
    else if (prefix <= LONG_STRING_BASE)  // [0x80, 0xb7] short string
    {
        const uint8_t len = prefix - SHORT_STRING_BASE;
        if (len >= input_len)
            return false;

        // Canonicality: a single byte < 0x80 must be encoded as itself, not as
        // a 1-byte string with the 0x81 prefix.
        if (len == 1 && input[1] < SHORT_STRING_BASE)
            return false;

        input.remove_prefix(1);
        out = {len, false};
        return true;
    }
    else if (prefix < SHORT_LIST_BASE)  // [0xb8, 0xbf] long string
    {
        const uint8_t len_of_str_len = prefix - LONG_STRING_BASE;
        if (len_of_str_len >= input_len)
            return false;

        // Canonicality: the encoded length must not have leading zero bytes.
        if (input[1] == 0)
            return false;

        const auto str_len = load<uint64_t>(input.substr(1, len_of_str_len));
        // Overflow-safe form of `1 + len_of_str_len + str_len > input_len`
        // (len_of_str_len < input_len is guaranteed above).
        if (str_len >= input_len - len_of_str_len)
            return false;

        // Canonicality: the long form is reserved for payloads longer than the short-form maximum.
        if (str_len <= SHORT_LENGTH_LIMIT)
            return false;

        input.remove_prefix(1 + len_of_str_len);
        out = {str_len, false};
        return true;
    }
    else if (prefix <= LONG_LIST_BASE)  // [0xc0, 0xf7] short list
    {
        const uint8_t list_len = prefix - SHORT_LIST_BASE;
        if (list_len >= input_len)
            return false;

        input.remove_prefix(1);
        out = {list_len, true};
        return true;
    }
    else  // [0xf8, 0xff] long list
    {
        const uint8_t len_of_list_len = prefix - LONG_LIST_BASE;
        if (len_of_list_len >= input_len)
            return false;

        // Canonicality: the encoded length must not have leading zero bytes.
        if (input[1] == 0)
            return false;

        const auto list_len = load<uint64_t>(input.substr(1, len_of_list_len));
        // Overflow-safe form of `1 + len_of_list_len + list_len > input_len`
        // (len_of_list_len < input_len is guaranteed above).
        if (list_len >= input_len - len_of_list_len)
            return false;

        // Canonicality: the long form is reserved for payloads longer than the short-form maximum.
        if (list_len <= SHORT_LENGTH_LIMIT)
            return false;

        input.remove_prefix(1 + len_of_list_len);
        out = {list_len, true};
        return true;
    }
}

bool decode(bytes_view& from, bytes& to) noexcept
{
    Header h;
    if (!decode_header(from, h) || h.is_list)
        return false;

    to = from.substr(0, h.payload_length);
    from.remove_prefix(h.payload_length);
    return true;
}

bool decode(bytes_view& from, evmc::bytes32& to) noexcept
{
    return decode(from, to.bytes);
}

bool decode(bytes_view& from, evmc::address& to) noexcept
{
    return decode(from, to.bytes);
}
}  // namespace evmone::rlp
