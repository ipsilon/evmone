// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "transaction.hpp"
#include "rlp_common.hpp"
#include "rlp_decode.hpp"
#include <algorithm>

namespace evmone::state
{
using intx::uint256;

namespace
{
[[nodiscard]] bool decode_authorization(bytes_view& from, Authorization& to) noexcept
{
    rlp::Header h;
    if (!rlp::decode_header(from, h) || !h.is_list || h.payload_length > from.size())
        return false;
    auto payload = from.substr(0, static_cast<size_t>(h.payload_length));
    from.remove_prefix(static_cast<size_t>(h.payload_length));

    if (!rlp::decode_fields(payload, to.chain_id, to.addr, to.nonce, to.v))
        return false;
    // EIP-7702 bounds y_parity to < 2**8; whether the value is in {0, 1} is a recovery-time check,
    // not a wire-format check. Accept any v in [0, 255] (recovery drops entries with v > 1) but
    // reject v >= 2**8 to match geth (V uint8) and revm/alloy (y_parity: U8).
    if (to.v >= 0x100)
        return false;
    return rlp::decode_fields(payload, to.r, to.s) && payload.empty();
}

/// Reads the RLP list header of the transaction body from @p from, advances @p from past the whole
/// list, and returns a view bounded to exactly the list payload in @p payload. Bounding to the
/// declared length (for both legacy and typed envelopes) makes the field decoders reject a list
/// shorter than its contents, and lets the caller detect data trailing the transaction.
[[nodiscard]] bool take_list_payload(bytes_view& from, bytes_view& payload) noexcept
{
    rlp::Header h;
    if (!rlp::decode_header(from, h) || !h.is_list || h.payload_length > from.size())
        return false;
    payload = from.substr(0, static_cast<size_t>(h.payload_length));
    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

[[nodiscard]] bool decode_transaction_body(bytes_view& from, Transaction& to) noexcept
{
    // A legacy transaction is a plain RLP list; a typed transaction (EIP-2718) is a type byte
    // followed by an RLP list. Decode the fields from a view bounded to the list payload; `from`
    // is left pointing just past the transaction.
    if (from.empty())
        return false;

    bytes_view body;
    if (from[0] >= rlp::SHORT_LIST_OFFSET)  // Legacy: the item is an RLP list.
    {
        to.type = Transaction::Type::legacy;
        if (!take_list_payload(from, body))
            return false;
    }
    else  // Typed: type byte followed by the RLP list.
    {
        uint8_t t{};
        if (!rlp::decode(from, t))
            return false;
        if (t > static_cast<uint8_t>(Transaction::Type::legacy) &&
            t <= static_cast<uint8_t>(Transaction::Type::set_code))
            to.type = static_cast<Transaction::Type>(t);
        else
            return false;

        if (!take_list_payload(from, body) || !rlp::decode(body, to.chain_id))
            return false;
    }

    if (!rlp::decode(body, to.nonce))
        return false;

    // Dynamic-fee transactions carry the priority fee per gas.
    if (to.type == Transaction::Type::eip1559 || to.type == Transaction::Type::blob ||
        to.type == Transaction::Type::set_code)
    {
        if (!rlp::decode(body, to.max_priority_gas_price))
            return false;
    }

    if (!rlp::decode(body, to.max_gas_price))
        return false;

    // Pre-EIP-1559 transactions have a single gas price used as both caps.
    if (to.type == Transaction::Type::legacy || to.type == Transaction::Type::access_list)
        to.max_priority_gas_price = to.max_gas_price;

    uint64_t gas_limit{};
    if (!rlp::decode(body, gas_limit))
        return false;
    to.gas_limit = static_cast<int64_t>(gas_limit);

    // The optional "to" address: an empty payload (0x80) marks a CREATE transaction; a 20-byte
    // payload sets the recipient.
    bytes to_payload;
    if (!rlp::decode(body, to_payload))
        return false;
    if (to_payload.empty())
        to.to = std::nullopt;
    else if (to_payload.size() == sizeof(evmc::address))
    {
        evmc::address to_addr;
        std::ranges::copy(to_payload, to_addr.bytes);
        to.to = to_addr;
    }
    else
        return false;

    if (!rlp::decode_fields(body, to.value, to.data))
        return false;

    if (to.type == Transaction::Type::legacy)
    {
        // The legacy signature v encodes the recovery id and, since EIP-155, the chain id.
        uint256 v_u256;
        if (!rlp::decode<uint256>(body, v_u256))
            return false;
        // Pre-EIP-155 transactions use v in {27, 28} and are not replay-protected (chain_id == 0).
        // Handle them explicitly, otherwise (v - 35) underflows and the chain id is garbage.
        if (v_u256 == 27 || v_u256 == 28)
        {
            to.v = (v_u256 == 28) ? 1 : 0;
            to.chain_id = 0;
        }
        else
        {
            to.v = (v_u256 - 35) % 2 == 0 ? 0 : 1;
            to.chain_id = ((v_u256 - 35 - to.v) / 2)[0];
        }
    }
    else
    {
        if (!rlp::decode(body, to.access_list))
            return false;
        if (to.type == Transaction::Type::blob)
        {
            if (!rlp::decode_fields(body, to.max_blob_gas_price, to.blob_hashes))
                return false;
        }
        else if (to.type == Transaction::Type::set_code)
        {
            bytes_view auth_payload;
            if (!take_list_payload(body, auth_payload))
                return false;
            while (!auth_payload.empty())
            {
                to.authorization_list.emplace_back();
                if (!decode_authorization(auth_payload, to.authorization_list.back()))
                    return false;
            }
        }
        if (!rlp::decode(body, to.v) || to.v > 1)
            return false;
    }

    return rlp::decode_fields(body, to.r, to.s) && body.empty();
}
}  // namespace

std::optional<Transaction> decode_transaction(bytes_view data) noexcept
{
    Transaction tx;
    if (!decode_transaction_body(data, tx) || !data.empty())
        return std::nullopt;  // Malformed transaction or trailing data.
    return tx;
}
}  // namespace evmone::state
