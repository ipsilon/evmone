// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "rlp.hpp"

namespace evmone::rlp
{

[[nodiscard]] Header decode_header(bytes_view& input)
{
    const auto input_len = input.size();

    if (input_len == 0)
        throw std::runtime_error("rlp decoding error: input is empty");

    const auto prefix = input[0];

    if (prefix < 0x80)
        return {1, false};
    else if (prefix < 0xb8)  // [0x80, 0xb7]
    {
        const uint8_t len = prefix - 0x80;
        if (len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");

        input.remove_prefix(1);
        return {static_cast<uint8_t>(prefix - 0x80), false};
    }
    else if (prefix < 0xc0)  // [0xb8, 0xbf]
    {
        const uint8_t len_of_str_len = prefix - 0xb7;
        if (len_of_str_len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");

        const auto str_len = evmone::rlp::load<uint64_t>(input.substr(1, len_of_str_len));
        if (str_len + len_of_str_len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");

        input.remove_prefix(1 + len_of_str_len);
        return {str_len, false};
    }
    else if (prefix < 0xf8)  // [0xc0, 0xf7]
    {
        const uint8_t list_len = prefix - 0xc0;
        if (list_len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");

        input.remove_prefix(1);
        return {list_len, true};
    }
    else  // [0xf8, 0xff]
    {
        const uint8_t len_of_list_len = prefix - 0xf7;
        if (len_of_list_len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");
        const auto list_len = evmone::rlp::load<uint64_t>(input.substr(1, len_of_list_len));
        if (list_len + len_of_list_len >= input_len)
            throw std::runtime_error("rlp decoding error: input too short");

        input.remove_prefix(1 + len_of_list_len);
        return {list_len, true};
    }
}

void decode(bytes_view& from, evmc::bytes32& to)
{
    decode(from, to.bytes);
}

void decode(bytes_view& from, bytes& to)
{
    const auto h = decode_header(from);

    if (h.is_list)
        throw std::runtime_error("rlp decoding error: unexpected list type");

    to = from.substr(0, static_cast<size_t>(h.payload_length));
    from.remove_prefix(static_cast<size_t>(h.payload_length));
}

void decode(bytes_view& from, evmc::address& to)
{
    decode(from, to.bytes);
}

}  // namespace evmone::rlp
