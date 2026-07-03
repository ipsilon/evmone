// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "transaction.hpp"
#include "rlp_common.hpp"
#include "rlp_decode.hpp"
#include <algorithm>
#include <limits>

namespace evmone::state
{
using intx::uint256;

namespace
{
/// Reads an RLP list header, advances @p from past the list, and returns its bounded payload.
[[nodiscard]] bool take_list_payload(bytes_view& from, bytes_view& payload) noexcept
{
    rlp::Header h;
    if (!rlp::decode_header(from, h) || !h.is_list)
        return false;
    payload = from.substr(0, static_cast<size_t>(h.payload_length));
    from.remove_prefix(static_cast<size_t>(h.payload_length));
    return true;
}

[[nodiscard]] bool decode_authorization(bytes_view& from, Authorization& to) noexcept
{
    bytes_view payload;
    if (!take_list_payload(from, payload))
        return false;

    if (!rlp::decode_fields(payload, to.chain_id, to.addr, to.nonce, to.v))
        return false;
    // EIP-7702 y_parity is a recovery-time check; accept any v < 2**8 (recovery rejects v > 1).
    if (to.v >= 0x100)
        return false;
    return rlp::decode_fields(payload, to.r, to.s) && payload.empty();
}

[[nodiscard]] bool decode_transaction_body(bytes_view& from, Transaction& to) noexcept
{
    // Legacy is a plain RLP list; a typed transaction (EIP-2718) is a type byte then an RLP list.
    // Fields are decoded from the list payload; `from` is left just past the transaction.
    if (from.empty())
        return false;

    bytes_view body;
    if (from[0] >= rlp::SHORT_LIST_BASE)  // Legacy: the item is an RLP list.
    {
        to.type = Transaction::Type::legacy;
        if (!take_list_payload(from, body))
            return false;
    }
    else  // Typed (EIP-2718): a raw type byte followed by the RLP list.
    {
        // The type is a single byte in [0x00, 0x7f], not an RLP item; reading it directly rejects
        // a non-canonical RLP-string form such as 0x81 0x02.
        const uint8_t t = from[0];
        from.remove_prefix(1);
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
    if (!rlp::decode(body, gas_limit) ||
        gas_limit > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return false;  // gas_limit must fit the signed Transaction::gas_limit.
    to.gas_limit = static_cast<int64_t>(gas_limit);

    // Empty "to" (0x80) is a CREATE transaction; otherwise a 20-byte recipient. The blob and
    // set-code types forbid the CREATE form, but that is enforced later in validate_transaction.
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
        // Legacy v encodes the recovery id and, since EIP-155, the chain id.
        uint256 v_u256;
        if (!rlp::decode<uint256>(body, v_u256))
            return false;
        if (v_u256 == 27 || v_u256 == 28)  // Pre-EIP-155: no chain id.
        {
            to.v = (v_u256 == 28) ? 1 : 0;
            to.chain_id = 0;
        }
        else if (v_u256 >= 35)  // EIP-155: v = 35 + 2 * chain_id + y_parity.
        {
            to.v = (v_u256 - 35) % 2 == 0 ? 0 : 1;
            const auto chain_id = (v_u256 - 35 - to.v) / 2;
            if (chain_id > std::numeric_limits<uint64_t>::max())
                return false;  // chain id must fit the 64-bit Transaction::chain_id.
            to.chain_id = static_cast<uint64_t>(chain_id);
        }
        else
            return false;  // Invalid legacy signature v.
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
