// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "statetest.hpp"
#include <test/state/mpt_hash.hpp>

namespace evmone::test
{
namespace
{
/// Converts EVM revision to the fork name commonly used in tests.
std::string_view to_test_fork_name(evmc_revision rev) noexcept
{
    switch (rev)
    {
    case EVMC_TANGERINE_WHISTLE:
        return "EIP150";
    case EVMC_SPURIOUS_DRAGON:
        return "EIP158";
    default:
        return evmc::to_string(rev);
    }
}
}  // namespace

[[nodiscard]] std::string get_invalid_tx_message(state::ErrorCode errc) noexcept
{
    using namespace state;
    switch (errc)
    {
    case SUCCESS:
        return "";
    case INTRINSIC_GAS_TOO_LOW:
        return "TR_IntrinsicGas";
    case TX_TYPE_NOT_SUPPORTED:
        return "TR_TypeNotSupported";
    case INSUFFICIENT_FUNDS:
        return "TR_NoFunds";
    case NONCE_HAS_MAX_VALUE:
        return "TR_NonceHasMaxValue:";
    case NONCE_TOO_HIGH:
        return "TR_NonceTooHigh";
    case NONCE_TOO_LOW:
        return "TR_NonceTooLow";
    case TIP_GT_FEE_CAP:
        return "TR_TipGtFeeCap";
    case FEE_CAP_LESS_THAN_BLOCKS:
        return "TR_FeeCapLessThanBlocks";
    case GAS_LIMIT_REACHED:
        return "TR_GasLimitReached";
    case SENDER_NOT_EOA:
        return "SenderNotEOA";
    case INIT_CODE_SIZE_LIMIT_EXCEEDED:
        return "TR_InitCodeLimitExceeded";
    case INIT_CODE_EMPTY:
        return "TR_InitCodeEmpty";
    case INIT_CODE_COUNT_LIMIT_EXCEEDED:
        return "TR_InitCodeCountLimitExceeded";
    case INIT_CODE_COUNT_ZERO:
        return "TR_InitCodeCountZero";
    case CREATE_BLOB_TX:
        return "TR_BLOBCREATE";
    case EMPTY_BLOB_HASHES_LIST:
        return "TR_EMPTYBLOB";
    case INVALID_BLOB_HASH_VERSION:
        return "TR_BLOBVERSION_INVALID";
    case BLOB_GAS_LIMIT_EXCEEDED:
        return "TR_BLOBLIST_OVERSIZE";
    case UNKNOWN_ERROR:
        return "Unknown error";
    default:
        assert(false);
        return "Wrong error code";
    }
}


json::json to_json(const TestState& state)
{
    json::json j = json::json::object();
    for (const auto& [addr, acc] : state)
    {
        auto& j_acc = j[hex0x(addr)];
        j_acc["nonce"] = hex0x(acc.nonce);
        j_acc["balance"] = hex0x(acc.balance);
        j_acc["code"] = hex0x(bytes_view(acc.code.data(), acc.code.size()));

        auto& j_storage = j_acc["storage"] = json::json::object();
        for (const auto& [key, val] : acc.storage)
        {
            if (!is_zero(val))
                j_storage[hex0x(key)] = hex0x(val);
        }
    }
    return j;
}

json::json to_state_test(std::string_view test_name, const state::BlockInfo& block,
    state::Transaction& tx, const TestState& pre, evmc_revision rev,
    const std::variant<state::TransactionReceipt, std::error_code>& res, const TestState& post)
{
    using state::Transaction;

    // FIXME: Move to common place.
    static constexpr auto SenderSecretKey =
        0x00000000000000000000000000000000000000000000000000000002b1263d2b_bytes32;

    json::json j;
    auto& jt = j[test_name];

    auto& jenv = jt["env"];
    jenv["currentNumber"] = hex0x(block.number);
    jenv["currentTimestamp"] = hex0x(block.timestamp);
    jenv["currentGasLimit"] = hex0x(block.gas_limit);
    jenv["currentCoinbase"] = hex0x(block.coinbase);
    jenv["currentBaseFee"] = hex0x(block.base_fee);
    jenv["currentRandom"] = hex0x(block.prev_randao);

    jt["pre"] = to_json(pre);

    auto& jtx = jt["transaction"];
    if (tx.to.has_value())
        jtx["to"] = hex0x(*tx.to);
    jtx["sender"] = hex0x(tx.sender);
    jtx["secretKey"] = hex0x(SenderSecretKey);
    jtx["nonce"] = hex0x(tx.nonce);
    if (tx.type >= Transaction::Type::eip1559)
    {
        jtx["maxFeePerGas"] = hex0x(tx.max_gas_price);
        jtx["maxPriorityFeePerGas"] = hex0x(tx.max_priority_gas_price);
    }
    else
    {
        assert(tx.max_gas_price == tx.max_priority_gas_price);
        jtx["gasPrice"] = hex0x(tx.max_gas_price);
    }

    jtx["data"][0] = hex0x(tx.data);
    jtx["gasLimit"][0] = hex0x(tx.gas_limit);
    jtx["value"][0] = hex0x(tx.value);

    // Force `accessLists` output even if empty.
    if (tx.type >= Transaction::Type::access_list)
        jtx["accessLists"][0] = json::json::array();

    if (!tx.access_list.empty())
    {
        auto& ja = jtx["accessLists"][0];
        for (const auto& [addr, storage_keys] : tx.access_list)
        {
            json::json je;
            je["address"] = hex0x(addr);
            auto& jstorage_keys = je["storageKeys"] = json::json::array();
            for (const auto& k : storage_keys)
                jstorage_keys.emplace_back(hex0x(k));
            ja.emplace_back(std::move(je));
        }
    }

    if (tx.type == Transaction::Type::blob)
    {
        jtx["maxFeePerBlobGas"] = hex0x(tx.max_blob_gas_price);
        jtx["blobVersionedHashes"] = json::json::array();
        for (const auto& blob_hash : tx.blob_hashes)
        {
            jtx["blobVersionedHashes"].emplace_back(hex0x(blob_hash));
        }
    }

    if (!tx.authorization_list.empty())
    {
        auto& ja = jtx["authorizationList"];
        for (const auto& [chain_id, addr, nonce, signer, r, s, y_parity] : tx.authorization_list)
        {
            json::json je;
            je["chainId"] = hex0x(chain_id);
            je["address"] = hex0x(addr);
            je["nonce"] = hex0x(nonce);
            je["v"] = hex0x(y_parity);
            je["r"] = hex0x(r);
            je["s"] = hex0x(s);
            if (signer.has_value())
                je["signer"] = hex0x(*signer);
            ja.emplace_back(std::move(je));
        }
    }


    auto& jpost = jt["post"][to_test_fork_name(rev)][0];
    jpost["indexes"] = {{"data", 0}, {"gas", 0}, {"value", 0}};
    jpost["hash"] = hex0x(mpt_hash(post));

    if (holds_alternative<std::error_code>(res))
    {
        jpost["expectException"] = get_invalid_tx_message(
            static_cast<state::ErrorCode>(std::get<std::error_code>(res).value()));
        jpost["logs"] = hex0x(logs_hash(std::vector<state::Log>()));
    }
    else
    {
        jpost["logs"] = hex0x(logs_hash(std::get<state::TransactionReceipt>(res).logs));
    }

    return j;
}
}  // namespace evmone::test
