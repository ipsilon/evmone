// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "block_transition.hpp"
#include <test/state/errors.hpp>
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
    if (!opts.skip_system_calls)
        system_call_block_start(block_state, block, block_hashes, rev, vm);

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

        auto res = transition(block_state, block, block_hashes, tx, rev, vm, block_gas_left,
            blob_gas_left, state_block_gas_left);

        if (holds_alternative<std::error_code>(res))
        {
            const auto ec = std::get<std::error_code>(res);
            rejected_txs.push_back({computed_tx_hash, i, ec.message()});
        }
        else
        {
            auto& receipt = get<state::TransactionReceipt>(res);

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
            auto block_end = system_call_block_end(block_state, block, block_hashes, rev, vm);
            if (const auto* ec = std::get_if<std::error_code>(&block_end))
                requests_error = *ec;
            else
                std::ranges::move(std::get<std::vector<state::Requests>>(block_end),
                    std::back_inserter(requests));
        }
    }

    finalize(block_state, rev, block.coinbase, opts.block_reward, block.ommers, block.withdrawals);

    const auto bloom = compute_bloom_filter(receipts);

    // EIP-8037/7778: block-level 2D gas formula: max(sum_regular, sum_state).
    const auto block_gas_used = rev >= EVMC_AMSTERDAM ?
                                    std::max(sum_regular_gas, sum_state_gas) :
                                    cumulative_gas_used;
    return {std::move(receipts), std::move(rejected_txs), std::move(requests), requests_error,
        block_gas_used, bloom, blob_gas_left, std::move(block_state)};
}
}  // namespace evmone::test
