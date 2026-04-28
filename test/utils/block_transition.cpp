// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "block_transition.hpp"
#include <test/state/errors.hpp>
#include <test/state/state.hpp>
#include <test/state/system_contracts.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>

namespace evmone::test
{
namespace
{
/// Redirects an ostream's streambuf for the scope's lifetime.
class StreamRedirect
{
    std::ostream& stream_;
    std::streambuf* prev_;

public:
    StreamRedirect(std::ostream& stream, std::streambuf* new_buf) noexcept
      : stream_{stream}, prev_{stream.rdbuf(new_buf)}
    {}

    StreamRedirect(const StreamRedirect&) = delete;
    StreamRedirect& operator=(const StreamRedirect&) = delete;

    ~StreamRedirect() { stream_.rdbuf(prev_); }
};
}  // namespace

TransitionResult apply_block(const TestState& state, evmc::VM& vm, const state::BlockInfo& block,
    const state::BlockHashes& block_hashes, const std::vector<state::Transaction>& txs,
    evmc_revision rev, int64_t blob_gas_limit, const BlockTransitionOptions& opts)
{
    const bool trace_enabled = static_cast<bool>(opts.open_trace);
    if (trace_enabled)
        vm.set_option("trace", "1");  // This actually appends a new tracer on each set_option().

    TestState block_state(state);

    // EIP-7928 BAL wiring: a BalStateView decorator forwards StateView calls to
    // `block_state` while capturing every cold storage-slot read. The builder
    // accumulates per-tx state diffs alongside those reads.
    state::BalBuilder bal_builder;
    state::BalStateView bal_view{block_state, bal_builder};
    constexpr auto tx_idx_pre = state::bal_tx_index::PRE_BLOCK;
    const auto tx_idx_post = state::bal_tx_index::post_block(txs.size());

    if (!opts.skip_system_calls)
    {
        auto diff = state::system_call_block_start(bal_view, block, block_hashes, rev, vm);
        bal_builder.record_diff(tx_idx_pre, diff, block_state);
        block_state.apply(diff);
    }

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;

    int64_t block_gas_left = block.gas_limit;
    // EIP-8037: separate state-gas budget tracker; only consulted by
    // validate_transaction for Amsterdam+.
    int64_t state_block_gas_left = block.gas_limit;
    int64_t cumulative_gas_used = 0;
    // EIP-8037: track regular and state gas separately for the block-level
    // max(sum_regular, sum_state) formula.
    int64_t sum_regular_gas = 0;
    int64_t sum_state_gas = 0;
    auto blob_gas_left = blob_gas_limit;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];
        const auto computed_tx_hash = keccak256(rlp::encode(tx));

        std::optional<StreamRedirect> trace_guard;
        if (trace_enabled)
            trace_guard.emplace(std::clog, opts.open_trace(i, computed_tx_hash).rdbuf());

        // EIP-7928: validate against the bare block_state, not bal_view. A tx
        // rejected at validation MUST NOT appear in the BAL, but bal_view's
        // get_account(sender) would otherwise record the sender unconditionally.
        const auto tx_props_or_err = state::validate_transaction(
            block_state, block, tx, rev, block_gas_left, blob_gas_left, state_block_gas_left);
        if (std::holds_alternative<std::error_code>(tx_props_or_err))
        {
            const auto ec = std::get<std::error_code>(tx_props_or_err);
            rejected_txs.push_back({computed_tx_hash, i, ec.message()});
        }
        else
        {
            // EIP-7928: run the tx through the BAL-recording state view and capture
            // its diff into the block access list.
            auto receipt = state::transition(bal_view, block, block_hashes, tx, rev, vm,
                std::get<state::TransactionProperties>(tx_props_or_err));

            const auto tx_idx = state::bal_tx_index::tx(i);
            bal_builder.record_diff(tx_idx, receipt.state_diff, block_state);
            block_state.apply(receipt.state_diff);

            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(block_state);

            // EIP-8037: accumulate the 2D components for the Amsterdam block-level
            // max(sum_regular, sum_state) formula. Pre-Amsterdam these components are
            // 0; the block then tracks the single gas dimension via receipt.gas_used.
            sum_regular_gas += receipt.regular_block_gas;
            sum_state_gas += receipt.state_block_gas;
            block_gas_left -=
                (rev >= EVMC_AMSTERDAM) ? receipt.regular_block_gas : receipt.gas_used;
            state_block_gas_left -= receipt.state_block_gas;
            blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
            receipts.emplace_back(std::move(receipt));
        }
    }

    std::vector<state::Requests> requests;
    std::error_code requests_error;
    if (!opts.skip_system_calls)
    {
        if (rev >= EVMC_PRAGUE)
        {
            if (auto opt_deposits = collect_deposit_requests(receipts); opt_deposits.has_value())
                requests.emplace_back(std::move(*opt_deposits));
            else
                requests_error = make_error_code(state::INVALID_DEPOSIT_EVENT_LAYOUT);
        }
        if (!requests_error)
        {
            auto block_end = state::system_call_block_end(bal_view, block, block_hashes, rev, vm);
            if (const auto* ec = std::get_if<std::error_code>(&block_end))
                requests_error = *ec;
            else
            {
                auto& rr = std::get<state::RequestsResult>(block_end);
                bal_builder.record_diff(tx_idx_post, rr.state_diff, block_state);
                block_state.apply(rr.state_diff);
                std::ranges::move(rr.requests, std::back_inserter(requests));
            }
        }
    }

    {
        auto fin_diff = state::finalize(
            bal_view, rev, block.coinbase, opts.block_reward, block.ommers, block.withdrawals);
        bal_builder.record_diff(tx_idx_post, fin_diff, block_state);
        block_state.apply(fin_diff);
    }

    const auto bloom = compute_bloom_filter(receipts);

    // EIP-7928: assemble the block access list and commit its hash.
    const auto bal = bal_builder.build();
    const auto bal_hash = bal.hash();
    const auto bal_gas_exceeded = bal.exceeds_gas_limit(static_cast<uint64_t>(block.gas_limit));

    // EIP-8037/7778: block-level 2D gas formula: max(sum_regular, sum_state).
    // Pre-Amsterdam blocks leave both sums at 0; fall back to cumulative_gas_used.
    const auto block_gas_used = (sum_regular_gas != 0 || sum_state_gas != 0) ?
                                    std::max(sum_regular_gas, sum_state_gas) :
                                    cumulative_gas_used;
    return {std::move(receipts), std::move(rejected_txs), std::move(requests), requests_error,
        block_gas_used, bloom, blob_gas_left, std::move(block_state), bal, bal_hash,
        bal_gas_exceeded};
}
}  // namespace evmone::test
