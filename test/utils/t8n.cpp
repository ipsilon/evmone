// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "t8n.hpp"
#include <nlohmann/json.hpp>
#include <test/state/ethash_difficulty.hpp>
#include <test/state/requests.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <test/utils/statetest.hpp>
#include <test/utils/utils.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace evmone::tooling
{
using JSON = nlohmann::json;
using namespace evmone::test;

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

void t8n(evmc::VM& vm, const T8NArgs& args)
{
    const auto rev = args.rev;

    const auto blob_params =
        (args.blob_params != nullptr) ?
            from_json<state::BlobParams>(JSON::parse(*args.blob_params, nullptr, false)) :
            get_blob_params(rev);

    TestState state;
    if (args.alloc != nullptr)
    {
        const auto j = JSON::parse(*args.alloc, nullptr, false);
        state = from_json<TestState>(j);
        validate_state(state, rev);
    }

    state::BlockInfo block;
    TestBlockHashes block_hashes;
    if (args.env != nullptr)
    {
        const auto j = JSON::parse(*args.env);
        block = from_json_with_rev(j, rev, blob_params);
        block_hashes = from_json<TestBlockHashes>(j);
    }

    JSON j_result;

    // Difficulty was received from upstream. No need to calc
    // TODO: Check if it's needed by the blockchain test. If not remove if statement true branch
    if (block.difficulty != 0)
    {
        j_result["currentDifficulty"] = hex0x(block.difficulty);
    }
    else
    {
        const auto current_difficulty = state::calculate_difficulty(block.parent_difficulty,
            block.parent_ommers_hash != EmptyListHash, block.parent_timestamp, block.timestamp,
            block.number, rev);

        j_result["currentDifficulty"] = hex0x(current_difficulty);
        block.difficulty = current_difficulty;

        if (rev < EVMC_PARIS)  // Override prev_randao with difficulty pre-Merge
            block.prev_randao = intx::be::store<bytes32>(intx::uint256{current_difficulty});
    }

    if (rev >= EVMC_LONDON)
        j_result["currentBaseFee"] = hex0x(block.base_fee);

    int64_t cumulative_gas_used = 0;
    auto blob_gas_left = static_cast<int64_t>(state::max_blob_gas_per_block(blob_params));
    std::vector<state::Transaction> transactions;
    std::vector<state::TransactionReceipt> receipts;
    int64_t block_gas_left = block.gas_limit;
    std::vector<state::Requests> requests;

    // Parse and execute transactions
    if (args.txs != nullptr)
    {
        const auto j_txs = JSON::parse(*args.txs);

        const bool trace_enabled = static_cast<bool>(args.open_trace);
        if (trace_enabled)
            vm.set_option("trace", "1");  // This actually appends new tracer each set_option().
        if (!args.opcode_count_file.empty())
            vm.set_option("opcode.count", args.opcode_count_file.c_str());

        std::vector<state::Log> txs_logs;

        if (j_txs.is_array())
        {
            j_result["receipts"] = JSON::array();
            j_result["rejected"] = JSON::array();

            if (!args.pre_state_only)
                system_call_block_start(state, block, block_hashes, rev, vm);

            for (size_t i = 0; i < j_txs.size(); ++i)
            {
                auto tx = from_json<state::Transaction>(j_txs[i]);
                tx.chain_id = args.chain_id;

                const auto computed_tx_hash = keccak256(rlp::encode(tx));
                const auto computed_tx_hash_str = hex0x(computed_tx_hash);

                if (j_txs[i].contains("hash"))
                {
                    const auto loaded_tx_hash_opt =
                        evmc::from_hex<bytes32>(j_txs[i]["hash"].get<std::string>());

                    if (!loaded_tx_hash_opt)
                        throw std::logic_error("transaction hash hex is malformed: " +
                                               j_txs[i]["hash"].get<std::string>());
                    if (*loaded_tx_hash_opt != computed_tx_hash)
                        throw std::logic_error("transaction hash mismatched: computed " +
                                               computed_tx_hash_str + ", expected " +
                                               hex0x(*loaded_tx_hash_opt));
                }

                std::optional<StreamRedirect> trace_guard;
                if (trace_enabled)
                    trace_guard.emplace(std::clog, args.open_trace(i, computed_tx_hash).rdbuf());

                auto res = transition(state, block, block_hashes, tx, rev, vm, block_gas_left,
                    blob_gas_left, block_gas_left);

                if (holds_alternative<std::error_code>(res))
                {
                    const auto ec = std::get<std::error_code>(res);
                    JSON j_rejected_tx;
                    j_rejected_tx["hash"] = computed_tx_hash_str;
                    j_rejected_tx["index"] = i;
                    j_rejected_tx["error"] = ec.message();
                    j_result["rejected"].push_back(j_rejected_tx);
                }
                else
                {
                    auto& receipt = get<state::TransactionReceipt>(res);

                    const auto& tx_logs = receipt.logs;

                    txs_logs.insert(txs_logs.end(), tx_logs.begin(), tx_logs.end());
                    auto& j_receipt = j_result["receipts"][j_result["receipts"].size()];

                    j_receipt["transactionHash"] = computed_tx_hash_str;
                    j_receipt["gasUsed"] = hex0x(static_cast<uint64_t>(receipt.gas_used));
                    cumulative_gas_used += receipt.gas_used;
                    receipt.cumulative_gas_used = cumulative_gas_used;
                    if (rev < EVMC_BYZANTIUM)
                        receipt.post_state = state::mpt_hash(state);
                    j_receipt["cumulativeGasUsed"] = hex0x(cumulative_gas_used);

                    j_receipt["blockHash"] = hex0x(bytes32{});
                    j_receipt["contractAddress"] = hex0x(address{});
                    j_receipt["logsBloom"] = hex0x(receipt.logs_bloom_filter);
                    j_receipt["logs"] = JSON::array();  // FIXME: Add to_json<state:Log>
                    j_receipt["root"] = "";
                    j_receipt["status"] = "0x1";
                    j_receipt["transactionIndex"] = hex0x(i);
                    blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
                    transactions.emplace_back(std::move(tx));
                    block_gas_left -= receipt.gas_used;
                    receipts.emplace_back(std::move(receipt));
                }
            }
        }

        if (!args.pre_state_only && rev >= EVMC_PRAGUE)
        {
            auto deposits_result = collect_deposit_requests(receipts);
            if (deposits_result.has_value())
                requests.emplace_back(std::move(*deposits_result));
            else
                // Report invalid block in the JSON result when deposit collection fails.
                j_result["blockException"] = "invalid deposit event layout";
            auto requests_result = system_call_block_end(state, block, block_hashes, rev, vm);
            if (requests_result.has_value())
                std::ranges::move(*requests_result, std::back_inserter(requests));
            else
                // Report invalid block in the JSON result when requests fail.
                j_result["blockException"] = "system contract empty or failed";
        }

        finalize(state, rev, block.coinbase, args.block_reward, block.ommers, block.withdrawals);

        j_result["logsHash"] = hex0x(logs_hash(txs_logs));
        j_result["stateRoot"] = hex0x(state::mpt_hash(state));
    }

    j_result["logsBloom"] = hex0x(compute_bloom_filter(receipts));
    j_result["receiptsRoot"] = hex0x(state::mpt_hash(receipts));
    if (rev >= EVMC_SHANGHAI)
        j_result["withdrawalsRoot"] = hex0x(state::mpt_hash(block.withdrawals));

    j_result["txRoot"] = hex0x(state::mpt_hash(transactions));
    j_result["gasUsed"] = hex0x(cumulative_gas_used);
    if (rev >= EVMC_CANCUN)
    {
        j_result["blobGasUsed"] =
            hex0x(static_cast<int64_t>(state::max_blob_gas_per_block(blob_params)) - blob_gas_left);
        if (block.excess_blob_gas.has_value())
            j_result["currentExcessBlobGas"] = hex0x(*block.excess_blob_gas);
    }
    if (rev >= EVMC_PRAGUE)
    {
        // EIP-7685: General purpose execution layer requests
        j_result["requests"] = JSON::array();
        for (const auto& r : requests)
        {
            if (!r.data().empty())
                // Only report non-empty requests. Include the leading type byte.
                j_result["requests"].emplace_back(hex0x(r.raw_data));
        }

        auto requests_hash = calculate_requests_hash(requests);

        j_result["requestsHash"] = hex0x(requests_hash);
    }

    if (args.out_result != nullptr)
        *args.out_result << std::setw(2) << j_result;
    if (args.out_alloc != nullptr)
        *args.out_alloc << std::setw(2) << to_json(TestState{state});
    if (args.out_body != nullptr)
        *args.out_body << hex0x(rlp::encode(transactions));
}
}  // namespace evmone::tooling
