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
#include <filesystem>
#include <fstream>
#include <ranges>
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

    const auto bloom = compute_bloom_filter(receipts);

    return {std::move(receipts), std::move(rejected_txs), std::move(requests), block_gas_used,
        bloom, blob_gas_left, std::move(block_state)};
}
}  // namespace

TestResult run_engine_test(const EngineTest& t, evmc::VM& vm)
{
    using namespace state;

    TestState current_state = t.pre_state;
    TestBlockHashes block_hashes{{0, t.genesis.hash}};
    hash256 last_accepted_block_hash = t.genesis.hash;

    struct ParentContext
    {
        uint64_t blob_gas_used = 0;
        uint64_t excess_blob_gas = 0;
        uint64_t base_fee_per_gas = 0;
        uint64_t gas_limit = 0;
        uint64_t gas_used = 0;
    };

    ParentContext parent_ctx{
        .blob_gas_used = t.genesis.blob_gas_used.value_or(0),
        .excess_blob_gas = t.genesis.excess_blob_gas.value_or(0),
        .base_fee_per_gas = t.genesis.base_fee_per_gas,
        .gas_limit = static_cast<uint64_t>(t.genesis.gas_limit),
        .gas_used = static_cast<uint64_t>(t.genesis.gas_used),
    };

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

        // 0. Pre-execution header validation.
        const auto header_error = [&]() -> std::optional<std::string> {
            // gas_limit window vs parent (Yellow Paper).
            const auto child_gas_limit = static_cast<uint64_t>(p.block_info.gas_limit);
            if (p.block_info.gas_limit < 5000)
                return "gas_limit below 5000 floor";
            if (child_gas_limit >= parent_ctx.gas_limit + parent_ctx.gas_limit / 1024)
                return "gas_limit exceeds parent + parent/1024";
            if (child_gas_limit <= parent_ctx.gas_limit - parent_ctx.gas_limit / 1024)
                return "gas_limit below parent - parent/1024";

            // base_fee_per_gas formula (London+).
            if (rev >= EVMC_LONDON)
            {
                const auto expected_base_fee = state::calc_base_fee(
                    static_cast<int64_t>(parent_ctx.gas_limit),
                    static_cast<int64_t>(parent_ctx.gas_used), parent_ctx.base_fee_per_gas);
                if (p.block_info.base_fee != expected_base_fee)
                    return "base_fee_per_gas mismatch (got " +
                           std::to_string(p.block_info.base_fee) + ", expected " +
                           std::to_string(expected_base_fee) + ")";
            }

            if (rev >= EVMC_CANCUN)
            {
                const auto blob_params =
                    get_blob_params(t.network, t.blob_schedule, p.block_info.timestamp);
                if (!p.block_info.excess_blob_gas.has_value() ||
                    !p.block_info.blob_gas_used.has_value())
                    return "missing blob gas fields";

                if (*p.block_info.blob_gas_used > state::max_blob_gas_per_block(blob_params))
                    return "blob_gas_used exceeds max_blob_gas_per_block";

                const auto parent_blob_base_fee =
                    state::compute_blob_gas_price(blob_params, parent_ctx.excess_blob_gas);
                const auto expected_excess = state::calc_excess_blob_gas(rev, blob_params,
                    parent_ctx.blob_gas_used, parent_ctx.excess_blob_gas,
                    parent_ctx.base_fee_per_gas, parent_blob_base_fee);
                if (*p.block_info.excess_blob_gas != expected_excess)
                    return "excess_blob_gas mismatch (got " +
                           std::to_string(*p.block_info.excess_blob_gas) + ", expected " +
                           std::to_string(expected_excess) + ")";
            }
            return std::nullopt;
        }();

        if (header_error.has_value())
        {
            if (expected_invalid)
                continue;
            return fail(*header_error);
        }

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

                // Verify execution requests (Prague+).
                // Per EIP-7685 the payload's params[3] lists only the non-empty
                // requests (each entry is <type_byte> || <data>), so we filter
                // out empty-data entries from the computed result before
                // comparing element-wise.
                if (rev >= EVMC_PRAGUE)
                {
                    std::vector<bytes_view> got;
                    got.reserve(res.requests->size());
                    for (const auto& r : *res.requests)
                    {
                        if (!r.data().empty())
                            got.emplace_back(r.raw_data);
                    }
                    if (got.size() != p.expected_requests.size())
                        return "requests count mismatch (got " + std::to_string(got.size()) +
                               ", expected " + std::to_string(p.expected_requests.size()) + ")";
                    for (size_t i = 0; i < got.size(); ++i)
                    {
                        if (got[i] != bytes_view{p.expected_requests[i]})
                            return "requests[" + std::to_string(i) + "] mismatch";
                    }
                }
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
            parent_ctx = {
                .blob_gas_used = p.block_info.blob_gas_used.value_or(0),
                .excess_blob_gas = p.block_info.excess_blob_gas.value_or(0),
                .base_fee_per_gas = p.block_info.base_fee,
                .gas_limit = static_cast<uint64_t>(p.block_info.gas_limit),
                .gas_used = static_cast<uint64_t>(res.gas_used),
            };
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

int run_engine_tests_path(
    const std::filesystem::path& path, evmc::VM& vm, std::ostream& out)
{
    namespace fs = std::filesystem;

    const auto read_file = [](const fs::path& file) {
        std::ifstream f{file};
        return std::string{
            std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
    };

    if (!fs::is_directory(path))
        return run_engine_tests_json(read_file(path), vm, out);

    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator{path})
    {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".json" &&
            entry.path().filename() != "index.json")
        {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);

    size_t total_failures = 0;
    size_t total_files_failed = 0;
    for (const auto& file : files)
    {
        out << "=== " << fs::relative(file, path).string() << " ===\n";
        const int rc = run_engine_tests_json(read_file(file), vm, out);
        if (rc > 0)
        {
            total_failures += static_cast<size_t>(rc);
            ++total_files_failed;
        }
    }
    out << "\nSUMMARY: " << (files.size() - total_files_failed) << "/" << files.size()
        << " files passed (" << total_failures << " individual test failures)\n";

    return static_cast<int>(std::min<size_t>(total_failures, 255));
}

}  // namespace evmone::test
