// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "mpt.hpp"
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

/// The uncle hash for a PoS block: keccak256(RLP([])).
constexpr auto EMPTY_UNCLE_HASH =
    0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347_bytes32;

/// CL gossip protocol max block size (EIP-7934).
constexpr size_t MAX_BLOCK_SIZE = 10ull * 1024 * 1024;
/// Safety margin for beacon block content (EIP-7934).
constexpr size_t SAFETY_MARGIN = 2ull * 1024 * 1024;
/// Maximum EL block size when RLP encoded (EIP-7934). 8 MB.
constexpr size_t MAX_RLP_BLOCK_SIZE = MAX_BLOCK_SIZE - SAFETY_MARGIN;

/// Fields needed to reconstruct an Osaka-era execution-layer block header
/// for the purpose of recomputing its block hash. PoS-only: difficulty=0,
/// nonce=0, ommers hash = EMPTY_UNCLE_HASH.
struct AssembledHeader
{
    hash256 parent_hash;
    address coinbase;
    hash256 state_root;
    hash256 transactions_root;
    hash256 receipts_root;
    state::BloomFilter logs_bloom;
    int64_t block_number;
    int64_t gas_limit;
    int64_t gas_used;
    int64_t timestamp;
    bytes extra_data;
    bytes32 prev_randao;
    uint64_t base_fee_per_gas;
    hash256 withdrawals_root;
    uint64_t blob_gas_used;
    uint64_t excess_blob_gas;
    hash256 parent_beacon_block_root;
    hash256 requests_hash;
};

/// Computes the transactions-trie root directly from the wire-format RLP
/// payloads carried by the engine payload. Using the raw bytes (rather than
/// re-encoding decoded `state::Transaction` values) preserves the canonical
/// signature `v` for legacy transactions — our decoder lowers `v` to a
/// y-parity bit and extracts chain_id separately, so a round-trip through
/// `rlp_encode(Transaction)` would produce a different (and incorrect) root.
hash256 transactions_root_from_rlp(std::span<const bytes> txs_rlp)
{
    state::MPT trie;
    for (size_t i = 0; i < txs_rlp.size(); ++i)
        trie.insert(rlp::encode(i), bytes{txs_rlp[i]});
    return trie.hash();
}

/// Encodes the canonical Ethereum mainnet block header as an RLP list,
/// returning the wrapped list bytes. Field order is per the yellow-paper /
/// post-Prague spec.
bytes encode_engine_header(const AssembledHeader& h)
{
    using namespace rlp;
    static const bytes nonce_8(8, 0x00);
    const uint64_t difficulty = 0;
    return encode_tuple(h.parent_hash, EMPTY_UNCLE_HASH, h.coinbase, h.state_root,
        h.transactions_root, h.receipts_root, bytes_view{h.logs_bloom}, difficulty,
        static_cast<uint64_t>(h.block_number), static_cast<uint64_t>(h.gas_limit),
        static_cast<uint64_t>(h.gas_used), static_cast<uint64_t>(h.timestamp), h.extra_data,
        h.prev_randao, bytes_view{nonce_8}, h.base_fee_per_gas, h.withdrawals_root,
        h.blob_gas_used, h.excess_blob_gas, h.parent_beacon_block_root, h.requests_hash);
}

/// Computes the canonical block RLP size (EIP-7934):
///   RLP([header, transactions, uncles=[], withdrawals]).
/// Legacy tx wire bytes (first byte >= 0xc0) embed as sub-lists; typed tx
/// wire bytes (first byte <= 0x7f, EIP-2718) are wrapped as opaque RLP byte
/// strings within the transactions list.
size_t compute_block_rlp_size(const bytes& header_encoded,
    const std::vector<bytes>& transactions_rlp,
    const std::vector<state::Withdrawal>& withdrawals)
{
    // Transactions list payload (concatenation of per-tx encodings).
    bytes tx_list_payload;
    for (const auto& tx_wire : transactions_rlp)
    {
        if (tx_wire.empty())
            continue;  // defensive
        if (tx_wire[0] >= 0xc0)
        {
            // Legacy: wire bytes are already an RLP list — embed directly.
            tx_list_payload.append(tx_wire);
        }
        else
        {
            // Typed (EIP-2718): wrap as opaque byte string.
            tx_list_payload.append(rlp::encode(bytes_view{tx_wire}));
        }
    }

    // Withdrawals list payload.
    bytes wd_list_payload;
    for (const auto& w : withdrawals)
        wd_list_payload.append(rlp_encode(w));

    // Sum the four element-encodings (each wrapped at its own list level):
    //   header_encoded is already wrapped.
    //   transactions: list_header + tx_list_payload
    //   uncles: single byte 0xc0 (empty list, post-Merge).
    //   withdrawals: list_header + wd_list_payload.
    const auto inner_size = header_encoded.size() +
        rlp::list_header_size(tx_list_payload.size()) + tx_list_payload.size() +
        1 +  // uncles
        rlp::list_header_size(wd_list_payload.size()) + wd_list_payload.size();

    // Outer block list framing.
    return rlp::list_header_size(inner_size) + inner_size;
}

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
        int64_t timestamp = 0;
        int64_t block_number = 0;
    };

    ParentContext parent_ctx{
        .blob_gas_used = t.genesis.blob_gas_used.value_or(0),
        .excess_blob_gas = t.genesis.excess_blob_gas.value_or(0),
        .base_fee_per_gas = t.genesis.base_fee_per_gas,
        .gas_limit = static_cast<uint64_t>(t.genesis.gas_limit),
        .gas_used = static_cast<uint64_t>(t.genesis.gas_used),
        .timestamp = t.genesis.timestamp,
        .block_number = t.genesis.block_number,
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
            // Sequential block number.
            if (p.block_info.number != parent_ctx.block_number + 1)
                return "block_number not sequential (got " +
                       std::to_string(p.block_info.number) + ", expected " +
                       std::to_string(parent_ctx.block_number + 1) + ")";

            // gas_used must not exceed gas_limit.
            if (p.block_info.gas_used > p.block_info.gas_limit)
                return "gas_used exceeds gas_limit";

            // Timestamp must strictly increase. Cast through uint64_t to mirror
            // blockchaintest_runner's handling of timestamps that don't fit
            // int64_t.
            if (static_cast<uint64_t>(p.block_info.timestamp) <=
                static_cast<uint64_t>(parent_ctx.timestamp))
                return "timestamp not strictly increasing";

            // extra_data must be <= 32 bytes.
            if (p.block_info.extra_data.size() > 32)
                return "extra_data exceeds 32 bytes";

            // Pre-Cancun: blob gas fields must be absent.
            if (rev < EVMC_CANCUN)
            {
                if (p.block_info.blob_gas_used.has_value() ||
                    p.block_info.excess_blob_gas.has_value())
                    return "blob gas fields present pre-Cancun";
            }

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

                // Verify block hash by reconstructing the EL header. This is
                // the canonical check real execution layers do: it catches
                // payloads whose declared blockHash diverges from the hash
                // implied by their other fields (e.g. tampered withdrawals
                // list — INVALID_WITHDRAWALS_ROOT / INVALID_BLOCK_HASH).
                const AssembledHeader header{
                    .parent_hash = p.block_info.parent_hash,
                    .coinbase = p.block_info.coinbase,
                    .state_root = mpt_hash(res.block_state),
                    .transactions_root = transactions_root_from_rlp(p.transactions_rlp),
                    .receipts_root = mpt_hash(res.receipts),
                    .logs_bloom = res.bloom,
                    .block_number = p.block_info.number,
                    .gas_limit = p.block_info.gas_limit,
                    .gas_used = res.gas_used,
                    .timestamp = p.block_info.timestamp,
                    .extra_data = p.block_info.extra_data,
                    .prev_randao = p.block_info.prev_randao,
                    .base_fee_per_gas = p.block_info.base_fee,
                    .withdrawals_root = mpt_hash(p.block_info.withdrawals),
                    .blob_gas_used = p.block_info.blob_gas_used.value_or(0),
                    .excess_blob_gas = p.block_info.excess_blob_gas.value_or(0),
                    .parent_beacon_block_root = p.block_info.parent_beacon_block_root,
                    .requests_hash = calculate_requests_hash(*res.requests),
                };
                const auto header_encoded = encode_engine_header(header);
                const auto computed_hash = keccak256(header_encoded);
                if (computed_hash != p.expected_block_hash)
                    return "block hash mismatch (got 0x" + hex(computed_hash) + ", expected 0x" +
                           hex(p.expected_block_hash) + ")";

                // EIP-7934 block RLP size limit (Osaka+).
                if (rev >= EVMC_OSAKA)
                {
                    const auto block_size = compute_block_rlp_size(
                        header_encoded, p.transactions_rlp, p.block_info.withdrawals);
                    if (block_size > MAX_RLP_BLOCK_SIZE)
                        return "block RLP size " + std::to_string(block_size) +
                               " exceeds limit " + std::to_string(MAX_RLP_BLOCK_SIZE);
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
                .timestamp = p.block_info.timestamp,
                .block_number = p.block_info.number,
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
