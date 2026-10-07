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

    // The block access list (EIP-7928) records the reads from the block state (the view)
    // and the state changes, applied to the block state, at their block access index.
    state::BalBuilder bal_builder;
    const state::BalStateView bal_view{block_state, bal_builder};
    const auto& view =
        rev >= EVMC_AMSTERDAM ? static_cast<const state::StateView&>(bal_view) : block_state;
    const auto apply = [&](size_t index, const state::StateDiff& diff) {
        if (rev >= EVMC_AMSTERDAM)
            bal_builder.record_diff(static_cast<uint32_t>(index), diff, block_state);
        block_state.apply(diff);
    };

    if (!opts.skip_system_calls)
        apply(0, state::system_call_block_start(view, block, block_hashes, rev, vm));

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;

    int64_t block_gas_left = block.gas_limit;
    int64_t block_state_gas_left = block.gas_limit;
    int64_t cumulative_gas_used = 0;
    auto blob_gas_left = blob_gas_limit;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];
        const auto computed_tx_hash = keccak256(rlp::encode(tx));

        std::optional<StreamRedirect> trace_guard;
        if (trace_enabled)
            trace_guard.emplace(std::clog, opts.open_trace(i, computed_tx_hash).rdbuf());

        // A rejected transaction is not in the block, so it is validated outside the view.
        const auto tx_props_or_err = state::validate_transaction(
            block_state, block, tx, rev, block_gas_left, block_state_gas_left, blob_gas_left);
        if (std::holds_alternative<std::error_code>(tx_props_or_err))
        {
            rejected_txs.push_back(
                {computed_tx_hash, i, std::get<std::error_code>(tx_props_or_err)});
        }
        else
        {
            auto receipt = state::transition(view, block, block_hashes, tx, rev, vm,
                std::get<state::TransactionProperties>(tx_props_or_err));
            apply(i + 1, receipt.state_diff);

            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(block_state);

            // Block gas accounting, with EIP-7778 applied in transition(). The execution
            // dimension is the block gas less the state one (EIP-8037).
            block_gas_left -= receipt.block_gas_used - receipt.state_gas_used;
            block_state_gas_left -= receipt.state_gas_used;
            blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
            receipts.emplace_back(std::move(receipt));
        }
    }

    // The withdrawals precede the system calls of the block end. Both are at the last index.
    apply(txs.size() + 1, state::finalize(view, rev, block.coinbase, opts.block_reward,
                              block.ommers, block.withdrawals));

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
            auto block_end = state::system_call_block_end(view, block, block_hashes, rev, vm);
            if (const auto* ec = std::get_if<std::error_code>(&block_end))
                requests_error = *ec;
            else
            {
                auto& rr = std::get<state::RequestsResult>(block_end);
                apply(txs.size() + 1, rr.state_diff);
                std::ranges::move(rr.requests, std::back_inserter(requests));
            }
        }
    }

    // Both counters start at block.gas_limit, so this is max(execution used, state used):
    // the block's gas used is its bottleneck dimension (EIP-8037).
    const auto block_gas_used = block.gas_limit - std::min(block_gas_left, block_state_gas_left);
    const auto bloom = compute_bloom_filter(receipts);

    return {std::move(receipts), std::move(rejected_txs), std::move(requests), requests_error,
        block_gas_used, bloom, blob_gas_left, std::move(block_state), bal_builder.build()};
}
}  // namespace evmone::test
