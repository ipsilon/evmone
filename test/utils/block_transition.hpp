// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <test/state/bal.hpp>
#include <test/state/bloom_filter.hpp>
#include <test/state/requests.hpp>
#include <test/state/transaction.hpp>
#include <test/utils/test_state.hpp>
#include <functional>
#include <iosfwd>
#include <optional>
#include <system_error>
#include <vector>

namespace evmone::state
{
struct BlockInfo;
}

namespace evmone::test
{
/// A transaction rejected during block application.
struct RejectedTransaction
{
    hash256 hash;  ///< keccak256 of the transaction's RLP encoding.
    size_t index;  ///< Position in the input transaction list.
    std::string message;
};

/// Options for apply_block(). Defaults match block-validation (full) behavior.
struct BlockTransitionOptions
{
    /// Mining reward paid to the coinbase on finalization (nullopt = PoS, no reward).
    std::optional<uint64_t> block_reward;

    /// Skip the block-start/block-end system calls and request collection
    /// (t8n pre-state-only mode). The transaction loop and finalization still run.
    bool skip_system_calls = false;

    /// Called once per transaction (just before execution) to obtain a per-tx
    /// trace sink; std::clog is redirected to the returned stream for the
    /// duration of that transaction. Unset = tracing disabled.
    std::function<std::ostream&(size_t tx_index, const hash256& tx_hash)> open_trace;
};

/// Result of applying a block; see apply_block().
struct TransitionResult
{
    std::vector<state::TransactionReceipt> receipts;  ///< Accepted transactions, in block order.
    std::vector<RejectedTransaction> rejected;        ///< Rejected transactions, in input order.
    std::vector<state::Requests> requests;            ///< Collected requests (EIP-7685).
    std::error_code requests_error;  ///< Set if requests collection failed (block is invalid).
    int64_t gas_used = 0;       ///< Block gas used; includes refunds for Amsterdam+ (EIP-7778).
    state::BloomFilter bloom;   ///< Logs bloom over all accepted transactions.
    int64_t blob_gas_left = 0;  ///< Blob gas remaining out of the budget passed in.
    TestState block_state;      ///< State after applying the block.
    /// Assembled block access list (EIP-7928). Its hash and the gas-limit item
    /// bound are derived on demand via hash() / exceeds_gas_limit().
    state::BlockAccessList block_access_list;
};

/// Applies a block of transactions to a copy of @p state: block-start system call, the
/// transactions, request collection, block-end system call, and finalization. The system calls
/// and request collection are skipped when `opts.skip_system_calls` is set.
///
/// Shared block-transition core for the blockchain test runner and the t8n tool. It performs
/// no validation/assertions and produces no output; callers interpret the returned result.
/// Block-level validity is assumed, but individual transactions may be rejected.
///
/// @param blob_gas_limit  The per-block blob-gas budget set by the protocol maximum.
[[nodiscard]] TransitionResult apply_block(const TestState& state, evmc::VM& vm,
    const state::BlockInfo& block, const state::BlockHashes& block_hashes,
    const std::vector<state::Transaction>& txs, evmc_revision rev, int64_t blob_gas_limit,
    const BlockTransitionOptions& opts = {});
}  // namespace evmone::test
