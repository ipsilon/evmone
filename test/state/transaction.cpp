// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "transaction.hpp"
#include "rlp_common.hpp"
#include "rlp_decode.hpp"
#include "test/utils/stdx/utility.hpp"

#include <limits>

namespace evmone::state
{
using intx::uint256;

bool decode(bytes_view& from, Authorization& to) noexcept
{
    bytes_view payload;
    if (!rlp::take_list_payload(from, payload))
        return false;

    if (!rlp::decode_multi(payload, to.chain_id, to.addr, to.nonce, to.v))
        return false;
    // EIP-7702 y_parity is a recovery-time check; accept any v < 2**8 (recovery rejects v > 1).
    if (to.v >= 0x100)
        return false;
    return rlp::decode_multi(payload, to.r, to.s) && payload.empty();
}

namespace
{
[[nodiscard]] bool decode_transaction_body(bytes_view& from, Transaction& to) noexcept
{
    if (from.empty()) [[unlikely]]
        return false;

    bytes_view body;
    if (from[0] >= rlp::SHORT_LIST_BASE)  // Legacy: the item is an RLP list.
    {
        to.type = Transaction::Type::legacy;
        if (!rlp::take_list_payload(from, body))
            return false;
    }
    else  // Typed (EIP-2718): a raw type byte followed by the RLP list.
    {
        // The type is a single byte in [0x00, 0x7f], not an RLP item; reading it directly rejects
        // a non-canonical RLP-string form such as 0x81 0x02.
        const auto t = from[0];
        from.remove_prefix(1);

        if (t == stdx::to_underlying(Transaction::Type::legacy) ||
            t > stdx::to_underlying(Transaction::Type::set_code)) [[unlikely]]
            return false;

        to.type = static_cast<Transaction::Type>(t);

        if (!rlp::take_list_payload(from, body) || !rlp::decode(body, to.chain_id))
            return false;
    }

    if (!rlp::decode(body, to.nonce))
        return false;

    // EIP-1559 and the later types (blob, set-code) carry a separate priority fee per gas;
    // earlier types reuse the single gas price for both caps (set below).
    const auto has_priority_gas_price = to.type >= Transaction::Type::eip1559;
    if (has_priority_gas_price)
    {
        if (!rlp::decode(body, to.max_priority_gas_price))
            return false;
    }

    if (!rlp::decode(body, to.max_gas_price))
        return false;

    if (!has_priority_gas_price)
        to.max_priority_gas_price = to.max_gas_price;

    uint64_t gas_limit{};
    if (!rlp::decode(body, gas_limit) ||
        gas_limit > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return false;  // gas_limit must fit the signed Transaction::gas_limit.
    to.gas_limit = static_cast<int64_t>(gas_limit);

    // Empty "to" (0x80) is a CREATE transaction; otherwise a 20-byte recipient. The blob and
    // set-code types forbid the CREATE form, but that is enforced later in validate_transaction.
    if (!body.empty() && body[0] == rlp::SHORT_STRING_BASE)  // Empty string.
    {
        to.to = std::nullopt;
        body.remove_prefix(1);
    }
    else
    {
        evmc::address recipient;
        if (!rlp::decode(body, recipient))  // Requires exactly 20 bytes.
            return false;
        to.to = recipient;
    }

    if (!rlp::decode_multi(body, to.value, to.data))
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
            if (!rlp::decode_multi(body, to.max_blob_gas_price, to.blob_hashes))
                return false;
        }
        else if (to.type == Transaction::Type::set_code)
        {
            if (!rlp::decode(body, to.authorization_list))
                return false;
        }
        if (!rlp::decode(body, to.v) || to.v > 1)
            return false;
    }

    return rlp::decode_multi(body, to.r, to.s) && body.empty();
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
