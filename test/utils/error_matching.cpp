// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "error_matching.hpp"
#include <test/state/errors.hpp>
#include <algorithm>

namespace evmone::test
{
namespace
{
/// The exceptions a fixture may name for a transaction evmone rejects with this error, on top of
/// the canonical name its error message carries.
struct AlternativeExceptions
{
    state::ErrorCode errc;   ///< The code evmone rejects the transaction with.
    std::string_view names;  ///< The other names the fixtures use for it, `|`-separated.
};

/// Where the execution specs draw more distinctions than evmone does, or draw one of them in a
/// different place. Both refuse the same transactions, so every name listed here is accepted.
constexpr AlternativeExceptions ALTERNATIVE_TX_EXCEPTIONS[]{
    // The specs make the floor cost (EIP-7623) a rule of its own; evmone folds it into the
    // intrinsic gas.
    {state::INTRINSIC_GAS_TOO_LOW, "TransactionException.INTRINSIC_GAS_BELOW_FLOOR_GAS_COST"},

    // The specs name the transaction type that arrived before its fork; evmone has one rule.
    {state::TYPE_NOT_SUPPORTED,
        "TransactionException.TYPE_1_TX_PRE_FORK|"
        "TransactionException.TYPE_2_TX_PRE_FORK|"
        "TransactionException.TYPE_3_TX_PRE_FORK|"
        "TransactionException.TYPE_4_TX_PRE_FORK"},

    // The specs separate the transaction's own blob count from the block's blob gas allowance.
    {state::BLOB_GAS_LIMIT_EXCEEDED,
        "TransactionException.TYPE_3_TX_MAX_BLOB_GAS_ALLOWANCE_EXCEEDED"},

    // evmone bounds the signature v while decoding the transaction, because the domain of v is
    // what tells a legacy transaction from a typed one and carries the chain id (EIP-155). The
    // execution specs read v as a plain integer and bound it with the rest of the signature.
    // decode_transaction() reports one code for every malformed encoding, so this accepts more
    // than the v rule; narrowing it needs the decoder to report the v domain separately.
    {state::INVALID_ENCODING, "TransactionException.INVALID_SIGNATURE_VRS"},
};

/// A retesteth `expectException` value and the evmone rejection(s) it stands for. Some legacy
/// names cover two rules at once, hence the second code.
struct LegacyException
{
    std::string_view name;
    state::ErrorCode errc;
    state::ErrorCode alt = state::SUCCESS;
};

constexpr LegacyException LEGACY_EXCEPTIONS[]{
    // Transaction-level, ethereum/tests and ethereum/legacytests.
    {"TR_IntrinsicGas", state::INTRINSIC_GAS_TOO_LOW},
    {"IntrinsicGas", state::INTRINSIC_GAS_TOO_LOW},
    {"TR_TypeNotSupported", state::TYPE_NOT_SUPPORTED},
    {"TR_NoFunds", state::INSUFFICIENT_ACCOUNT_FUNDS},
    {"TR_NoFundsX", state::INSUFFICIENT_ACCOUNT_FUNDS},
    {"TR_NoFundsOrGas", state::INSUFFICIENT_ACCOUNT_FUNDS, state::INTRINSIC_GAS_TOO_LOW},
    {"SenderNotEOA", state::SENDER_NOT_EOA},
    {"SenderNotEOAorNoCASH", state::SENDER_NOT_EOA, state::INSUFFICIENT_ACCOUNT_FUNDS},
    {"TR_GasLimitReached", state::GAS_ALLOWANCE_EXCEEDED},
    {"TR_TipGtFeeCap", state::PRIORITY_GREATER_THAN_MAX_FEE_PER_GAS},
    {"TR_FeeCapLessThanBlocks", state::INSUFFICIENT_MAX_FEE_PER_GAS},
    {"TR_FeeCapLessThanBlocksORNoFunds", state::INSUFFICIENT_MAX_FEE_PER_GAS,
        state::INSUFFICIENT_ACCOUNT_FUNDS},
    {"TR_FeeCapLessThanBlocksORGasLimitReached", state::INSUFFICIENT_MAX_FEE_PER_GAS,
        state::GAS_ALLOWANCE_EXCEEDED},
    {"TR_NonceHasMaxValue", state::NONCE_IS_MAX},
    {"TR_NonceTooLow", state::NONCE_TOO_LOW},
    {"TR_NonceTooHigh", state::NONCE_TOO_HIGH},
    {"TR_RLP_WRONGVALUE", state::INVALID_ENCODING},
    {"TR_InitCodeLimitExceeded", state::INITCODE_SIZE_EXCEEDED},
    {"TR_BLOBCREATE", state::CREATE_BLOB_TX},
    {"TR_EMPTYBLOB", state::EMPTY_BLOB_HASHES_LIST},
    {"TR_BLOBVERSION_INVALID", state::INVALID_BLOB_HASH_VERSION},
    {"TR_BLOBLIST_OVERSIZE", state::BLOB_GAS_LIMIT_EXCEEDED},

    // Block-level. The first three are spec names, not retesteth ones: they name rules evmone
    // does not tell apart, so the fixture's value is replaced with the one evmone reports.
    {"BlockException.IMPORT_IMPOSSIBLE_UNCLES_OVER_PARIS", state::INCORRECT_BLOCK_FORMAT},
    {"BlockException.GAS_USED_OVERFLOW", state::INCORRECT_BLOCK_FORMAT},
    {"BlockException.RLP_STRUCTURES_ENCODING|BlockException.RLP_INVALID_FIELD_OVERFLOW_64",
        state::INCORRECT_BLOCK_FORMAT},
    {"PostParisUncleHashIsNotEmpty", state::INCORRECT_BLOCK_FORMAT},
    {"3675PreParis1559BlockRejected", state::INCORRECT_BLOCK_FORMAT},
    {"InvalidNumber", state::INCORRECT_BLOCK_FORMAT},
    {"InvalidTimestampOlderParent", state::INVALID_BLOCK_TIMESTAMP_OLDER_THAN_PARENT},
    {"TooMuchGasUsed", state::INCORRECT_BLOCK_FORMAT},
    {"UncleParentIsNotAncestor", state::INCORRECT_BLOCK_FORMAT},
    {"InvalidGasLimit2", state::INVALID_GASLIMIT},
    {"1559BlockImportImpossible_BaseFeeWrong", state::INVALID_BASEFEE_PER_GAS},
};

/// Takes the next `|`-separated name off @p list, which is left pointing past it.
std::string_view take_name(std::string_view& list) noexcept
{
    const auto end = std::min(list.find('|'), list.size());
    const auto name = list.substr(0, end);
    list.remove_prefix(std::min(end + 1, list.size()));
    return name;
}
}  // namespace

std::string map_legacy_exception(std::string_view expected)
{
    const auto it = std::ranges::find(LEGACY_EXCEPTIONS, expected, &LegacyException::name);
    if (it == std::end(LEGACY_EXCEPTIONS))
        return std::string{expected};

    auto names = make_error_code(it->errc).message();
    if (it->alt != state::SUCCESS)
        names += '|' + make_error_code(it->alt).message();
    return names;
}

bool contains_any(std::string_view expected, std::string_view names) noexcept
{
    while (!names.empty())
    {
        const auto name = take_name(names);
        for (auto rest = expected; !rest.empty();)
        {
            if (take_name(rest) == name)
                return true;
        }
    }
    return false;
}

bool is_expected_tx_exception(const std::error_code& ec, std::string_view expected) noexcept
{
    if (contains_any(expected, ec.message()))  // The message is the canonical exception name.
        return true;

    return std::ranges::any_of(ALTERNATIVE_TX_EXCEPTIONS, [&](const AlternativeExceptions& a) {
        return make_error_code(a.errc) == ec && contains_any(expected, a.names);
    });
}
}  // namespace evmone::test
