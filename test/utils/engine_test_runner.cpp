// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "mpt_hash.hpp"
#include "rlp.hpp"
#include "rlp_encode.hpp"
#include <test/state/requests.hpp>
#include <test/state/state.hpp>
#include <test/utils/rlp_decode.hpp>
#include <algorithm>
#include <sstream>

namespace evmone::test
{

namespace
{
struct RejectedTransaction
{
    hash256 hash;
    size_t index;
    std::string message;
};

struct BlockResult
{
    std::vector<state::TransactionReceipt> receipts;
    std::vector<RejectedTransaction> rejected;
    std::optional<std::vector<state::Requests>> requests;
    int64_t gas_used;
    state::BloomFilter bloom;
    int64_t blob_gas_left;
    TestState block_state;
};

// Modeled on blockchaintest_runner.cpp:apply_block. Returns a BlockResult.
// Transactions are pre-decoded (by the caller) into state::Transaction values.
BlockResult apply_block(
    const TestState& state, evmc::VM& vm, const state::BlockInfo& block,
    const state::BlockHashes& block_hashes,
    const std::vector<state::Transaction>& txs, evmc_revision rev)
{
    TestState block_state(state);
    system_call_block_start(block_state, block, block_hashes, rev, vm);

    int64_t block_gas_left = block.gas_limit;
    auto blob_gas_left = static_cast<int64_t>(block.blob_gas_used.value_or(0));

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;
    int64_t cumulative_gas_used = 0;
    int64_t block_gas_used = 0;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];
        const auto tx_hash = keccak256(rlp::encode(tx));
        auto res = transition(
            block_state, block, block_hashes, tx, rev, vm, block_gas_left, blob_gas_left);
        if (std::holds_alternative<std::error_code>(res))
        {
            rejected_txs.push_back({tx_hash, i, std::get<std::error_code>(res).message()});
        }
        else
        {
            auto& receipt = std::get<state::TransactionReceipt>(res);
            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(block_state);

            const auto block_tx_gas = (rev >= EVMC_AMSTERDAM) ?
                                          receipt.gas_used + receipt.gas_refund :
                                          receipt.gas_used;
            block_gas_used += block_tx_gas;
            block_gas_left -= block_tx_gas;
            blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
            receipts.emplace_back(std::move(receipt));
        }
    }

    auto requests = [&]() -> std::optional<std::vector<state::Requests>> {
        std::vector<state::Requests> collected;
        if (rev >= EVMC_PRAGUE)
        {
            auto opt_deposits = collect_deposit_requests(receipts);
            if (!opt_deposits.has_value())
                return std::nullopt;
            collected.emplace_back(std::move(*opt_deposits));
        }
        auto sysreq = system_call_block_end(block_state, block, block_hashes, rev, vm);
        if (!sysreq.has_value())
            return std::nullopt;
        std::ranges::move(*sysreq, std::back_inserter(collected));
        return collected;
    }();

    finalize(block_state, rev, block.coinbase, std::nullopt, block.ommers, block.withdrawals);

    return {std::move(receipts), std::move(rejected_txs), std::move(requests), block_gas_used,
        compute_bloom_filter(receipts), blob_gas_left, std::move(block_state)};
}
}  // namespace

TestResult run_engine_test(const EngineTest& t, evmc::VM& vm)
{
    using namespace state;

    TestState current_state = t.pre_state;
    TestBlockHashes block_hashes{{0, t.genesis.hash}};
    hash256 last_accepted_block_hash = t.genesis.hash;

    for (size_t pi = 0; pi < t.payloads.size(); ++pi)
    {
        const auto& p = t.payloads[pi];
        const auto rev = t.rev.get_revision(p.block_info.timestamp);

        const auto fail = [&](std::string_view what) -> TestResult {
            std::ostringstream os;
            os << "payload " << pi << ": " << what;
            return {false, os.str()};
        };
        const auto expected_invalid = p.validation_error.has_value();

        // 1. Decode transactions.
        std::vector<Transaction> txs;
        txs.reserve(p.transactions_rlp.size());
        for (size_t ti = 0; ti < p.transactions_rlp.size(); ++ti)
        {
            try
            {
                Transaction tx;
                bytes_view view{p.transactions_rlp[ti]};
                rlp_decode(view, tx);
                txs.emplace_back(std::move(tx));
            }
            catch (const std::exception& ex)
            {
                if (expected_invalid)
                    goto next_payload;
                std::ostringstream os;
                os << "tx " << ti << " RLP decode error: " << ex.what();
                return fail(os.str());
            }
        }

        // 2. Recover senders.
        for (size_t ti = 0; ti < txs.size(); ++ti)
        {
            auto sender = recover_sender(txs[ti]);
            if (!sender.has_value())
            {
                if (expected_invalid)
                    goto next_payload;
                std::ostringstream os;
                os << "tx " << ti << " sender recovery failed";
                return fail(os.str());
            }
            txs[ti].sender = *sender;
        }

        {
            // 3. Apply the block.
            auto res = apply_block(current_state, vm, p.block_info, block_hashes, txs, rev);

            // 4. Verify expected outputs.
            const auto verify_failed = [&]() -> std::optional<std::string> {
                if (!res.requests.has_value())
                    return "system requests collection failed";
                if (!res.rejected.empty())
                    return "transaction rejected: " + res.rejected[0].message;
                if (res.blob_gas_left != 0)
                    return "blob gas mismatch (left=" + std::to_string(res.blob_gas_left) + ")";
                if (mpt_hash(res.block_state) != p.expected_state_root)
                    return "state root mismatch";
                if (mpt_hash(res.receipts) != p.expected_receipts_root)
                    return "receipts root mismatch";
                if (bytes_view{res.bloom} != bytes_view{p.expected_logs_bloom})
                    return "logs bloom mismatch";
                if (res.gas_used != p.expected_gas_used)
                    return "gas used mismatch (got " + std::to_string(res.gas_used) +
                           ", expected " + std::to_string(p.expected_gas_used) + ")";
                return std::nullopt;
            }();

            if (verify_failed.has_value())
            {
                if (expected_invalid)
                    goto next_payload;
                return fail(*verify_failed);
            }

            if (expected_invalid)
                return fail("expected validation error '" + *p.validation_error +
                            "' but block was accepted");

            // Advance chain head.
            current_state = std::move(res.block_state);
            block_hashes[p.block_info.number] = p.expected_block_hash;
            last_accepted_block_hash = p.expected_block_hash;
        }

    next_payload:
        continue;
    }

    // Final checks.
    if (state::mpt_hash(current_state) != state::mpt_hash(t.post_state))
        return {false, "final: post state mismatch"};

    if (last_accepted_block_hash != t.last_block_hash)
        return {false, "final: lastblockhash differs from last accepted chain head"};

    return {true, ""};
}

int run_engine_tests_json(std::string_view json, evmc::VM& vm, std::ostream& out)
{
    std::vector<EngineTest> tests;
    try
    {
        tests = load_engine_tests(json);
    }
    catch (const std::exception& ex)
    {
        out << "ERROR: " << ex.what() << "\n";
        return 1;
    }

    size_t failures = 0;
    for (const auto& t : tests)
    {
        const auto r = run_engine_test(t, vm);
        if (r.passed)
        {
            out << "PASS " << t.name << "\n";
        }
        else
        {
            ++failures;
            out << "FAIL " << t.name << "\n  " << r.error << "\n";
        }
    }
    out << (tests.size() - failures) << "/" << tests.size() << " passed\n";

    return static_cast<int>(std::min<size_t>(failures, 255));
}

}  // namespace evmone::test
