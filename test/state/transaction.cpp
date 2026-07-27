// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "transaction.hpp"
// TODO: The RLP encoder belongs in the state library, see the note in authorization.cpp.
#include "../utils/rlp.hpp"
#include "../utils/stdx/utility.hpp"
#include "hash_utils.hpp"
#include "rlp_decode.hpp"
#include <evmone_precompiles/secp256k1.hpp>

namespace evmone::state
{
bool decode(bytes_view& from, Authorization& to) noexcept
{
    bytes_view payload;
    if (!rlp::take_list_payload(from, payload))
        return false;

    // An out-of-range y_parity only invalidates the authorization, so anything fitting its uint8_t
    // is accepted here; recover_authority rejects the values above 1.
    return rlp::decode_multi(payload, to.chain_id, to.addr, to.nonce, to.y_parity, to.r, to.s) &&
           payload.empty();
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
        address recipient;
        if (!rlp::decode(body, recipient))  // Requires exactly 20 bytes.
            return false;
        to.to = recipient;
    }

    if (!rlp::decode_multi(body, to.value, to.data))
        return false;

    if (to.type == Transaction::Type::legacy)
    {
        // Legacy v carries the recovery id and, since EIP-155, the chain id. It is kept verbatim,
        // the way rlp_encode() writes it, and the chain id derived alongside. Requiring the whole
        // v to fit uint64_t bounds the chain id to 2**63 - 18, the same limit the JSON transaction
        // loader has.
        if (!rlp::decode(body, to.v))
            return false;

        if (to.v >= 35)  // EIP-155: v = 35 + 2 * chain_id + y_parity.
            to.chain_id = (to.v - 35) / 2;
        else if (to.v != 27 && to.v != 28)  // Pre-EIP-155: bound to no chain, chain_id unused.
            return false;
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

std::optional<address> recover_sender(const Transaction& tx, bytes_view txbytes) noexcept
{
    // The signing preimage is the transaction's encoding without the trailing (v, r, s).
    const auto typed = tx.type != Transaction::Type::legacy;
    auto envelope = txbytes.substr(typed ? 1 : 0);  // Skip the EIP-2718 type byte.
    bytes_view payload;
    [[maybe_unused]] const auto is_list = rlp::take_list_payload(envelope, payload);
    assert(is_list);  // tx has been decoded from txbytes, so its list header is valid.

    // Find the length of the encoded signature to find the preimage length.
    // The decoder accepts only canonical integers, so re-encoding (v, r, s) gives their wire sizes.
    const auto signature_size =
        rlp::encode(tx.v).size() + rlp::encode(tx.r).size() + rlp::encode(tx.s).size();
    assert(signature_size <= payload.size());
    auto preimage = bytes{payload.substr(0, payload.size() - signature_size)};

    // Protected legacy transactions sign chain_id by appending (chain_id, 0, 0) (EIP-155).
    // TODO: Allocate bytes only in this case; use views for typed transactions.
    if (!typed && tx.chain_id_protected())
        preimage += rlp::encode(tx.chain_id) + rlp::encode(uint64_t{}) + rlp::encode(uint64_t{});

    // A typed v is {0, 1}. A legacy v is 27 + y_parity, or 35 + 2 * chain_id + y_parity (EIP-155):
    // both bases are odd, so an even v means y_parity 1.
    const auto y_parity = typed ? tx.v != 0 : tx.v % 2 == 0;

    const auto h = keccak256((typed ? bytes{stdx::to_underlying(tx.type)} : bytes{}) +
                             rlp::internal::wrap_list(preimage));
    const auto r_bytes = intx::be::store<bytes32>(tx.r);
    const auto s_bytes = intx::be::store<bytes32>(tx.s);
    return evmmax::secp256k1::ecrecover(
        h.bytes, r_bytes.bytes, s_bytes.bytes, y_parity, evmmax::secp256k1::RecoveryMode::strict);
}
}  // namespace evmone::state
