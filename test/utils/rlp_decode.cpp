// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "rlp_decode.hpp"
#include "rlp.hpp"
#include "stdx/utility.hpp"

namespace evmone::state
{
using namespace rlp;
using intx::uint256;

void rlp_decode(bytes_view& from, Transaction& to)
{
    const auto h = decode_header(from);

    // Legacy type starts with a list.
    if (h.is_list)
        to.type = Transaction::Type::legacy;
    else
    {
        // Decode tx type for type > Transaction::Type::legacy.
        uint8_t t{};
        decode(from, t);

        if (t > stdx::to_underlying(Transaction::Type::legacy) &&
            t <= stdx::to_underlying(Transaction::Type::eip1559))
            to.type = static_cast<Transaction::Type>(t);
        else
            throw std::runtime_error("rlp decoding error: unexpected transaction type.");

        // Decode list after type identifier.
        if (!decode_header(from).is_list)
            throw std::runtime_error("rlp decoding error: unexpected type. list expected");

        decode(from, to.chain_id);
    }

    decode(from, to.nonce);

    // Decode max priority fee per gas.
    if (to.type == Transaction::Type::eip1559)
        rlp::decode(from, to.max_priority_gas_price);

    decode(from, to.max_gas_price);

    // Init max_priority_gas_price as max_gas_price for pre-eip1559.
    if (to.type != Transaction::Type::eip1559)
        to.max_priority_gas_price = to.max_gas_price;

    uint64_t gas_limit{};
    decode(from, gas_limit);
    to.gas_limit = static_cast<int64_t>(gas_limit);

    // Init address field. It's std::optional.
    to.to = evmc::address{};
    decode(from, to.to->bytes);
    decode(from, to.value);
    decode(from, to.data);

    // For legacy tx chain id is encoded in `v` value.
    if (to.type == Transaction::Type::legacy)
    {
        uint256 v_u256;
        decode<uint256>(from, v_u256);
        to.v = (v_u256 - 35) % 2 == 0 ? 0 : 1;
        to.chain_id = ((v_u256 - 35 - to.v) / 2)[0];
    }
    else
    {
        decode(from, to.access_list);
        decode(from, to.v);
        if (to.v > 1)
            throw std::runtime_error("rlp decoding error: invalid y_parity value");
    }

    decode(from, to.r);
    decode(from, to.s);
}

}  // namespace evmone::state
