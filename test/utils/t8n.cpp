// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "t8n.hpp"
#include <nlohmann/json.hpp>
#include <test/state/ethash_difficulty.hpp>
#include <test/state/requests.hpp>
#include <test/utils/block_transition.hpp>
#include <test/utils/mpt_hash.hpp>
#include <test/utils/rlp.hpp>
#include <test/utils/rlp_encode.hpp>
#include <test/utils/statetest.hpp>
#include <test/utils/utils.hpp>
#include <iomanip>
#include <stdexcept>

namespace evmone::tooling
{
using JSON = nlohmann::json;
using namespace evmone::test;

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

    const auto blob_gas_limit = static_cast<int64_t>(state::max_blob_gas_per_block(blob_params));
    int64_t gas_used = 0;
    int64_t blob_gas_left = blob_gas_limit;
    std::vector<state::Transaction> transactions;
    std::vector<state::TransactionReceipt> receipts;
    std::vector<state::Requests> requests;
    state::BloomFilter bloom{};
    TestState post_state;

    // Parse and execute transactions
    if (args.txs != nullptr)
    {
        const auto j_txs = JSON::parse(*args.txs);

        if (!args.opcode_count_file.empty())
            vm.set_option("opcode.count", args.opcode_count_file.c_str());

        j_result["receipts"] = JSON::array();
        j_result["rejected"] = JSON::array();

        // Parse the transactions, assign the chain ID and validate any provided hash. A non-array
        // `txs` value yields zero transactions but still produces a full, finalized block result.
        std::vector<state::Transaction> txs;
        if (j_txs.is_array())
        {
            txs.reserve(j_txs.size());
            for (const auto& j_tx : j_txs)
            {
                auto tx = from_json<state::Transaction>(j_tx);
                tx.chain_id = args.chain_id;

                if (j_tx.contains("hash"))
                {
                    const auto computed_tx_hash = keccak256(rlp::encode(tx));
                    const auto loaded_tx_hash_opt =
                        evmc::from_hex<bytes32>(j_tx["hash"].get<std::string>());
                    if (!loaded_tx_hash_opt)
                        throw std::logic_error("transaction hash hex is malformed: " +
                                               j_tx["hash"].get<std::string>());
                    if (*loaded_tx_hash_opt != computed_tx_hash)
                        throw std::logic_error("transaction hash mismatched: computed " +
                                               hex0x(computed_tx_hash) + ", expected " +
                                               hex0x(*loaded_tx_hash_opt));
                }

                txs.emplace_back(std::move(tx));
            }
        }

        auto res = apply_block(state, vm, block, block_hashes, txs, rev, blob_gas_limit,
            {.block_reward = args.block_reward,
                .skip_system_calls = args.pre_state_only,
                .open_trace = args.open_trace});

        // Build the receipts/rejected JSON lists from the partitioned result. `rejected`
        // is in input order, so walk it alongside the input transactions; the rest map to
        // `receipts` in block order.
        std::vector<state::Log> txs_logs;
        auto rejected_it = res.rejected.begin();
        size_t receipt_index = 0;
        for (size_t i = 0; i < txs.size(); ++i)
        {
            if (rejected_it != res.rejected.end() && rejected_it->index == i)
            {
                JSON j_rejected_tx;
                j_rejected_tx["hash"] = hex0x(rejected_it->hash);
                j_rejected_tx["index"] = i;
                j_rejected_tx["error"] = rejected_it->message;
                j_result["rejected"].push_back(j_rejected_tx);
                ++rejected_it;
            }
            else
            {
                const auto& receipt = res.receipts[receipt_index++];
                txs_logs.insert(txs_logs.end(), receipt.logs.begin(), receipt.logs.end());

                auto& j_receipt = j_result["receipts"][j_result["receipts"].size()];
                j_receipt["transactionHash"] = hex0x(keccak256(rlp::encode(txs[i])));
                j_receipt["gasUsed"] = hex0x(static_cast<uint64_t>(receipt.gas_used));
                j_receipt["cumulativeGasUsed"] = hex0x(receipt.cumulative_gas_used);
                j_receipt["blockHash"] = hex0x(bytes32{});
                j_receipt["contractAddress"] = hex0x(address{});
                j_receipt["logsBloom"] = hex0x(receipt.logs_bloom_filter);
                j_receipt["logs"] = JSON::array();  // FIXME: Add to_json<state:Log>
                j_receipt["root"] = "";
                j_receipt["status"] = "0x1";
                j_receipt["transactionIndex"] = hex0x(i);
                transactions.emplace_back(std::move(txs[i]));
            }
        }

        if (res.requests_error)
            // Report invalid block in the JSON result when request collection fails.
            j_result["blockException"] = res.requests_error.message();
        else
            requests = std::move(res.requests);

        receipts = std::move(res.receipts);
        // Block gas used reported as the cumulative transaction gas (refunds excluded),
        // preserving the prior t8n output.
        gas_used = receipts.empty() ? 0 : receipts.back().cumulative_gas_used;
        bloom = res.bloom;
        blob_gas_left = res.blob_gas_left;
        post_state = std::move(res.block_state);

        j_result["logsHash"] = hex0x(logs_hash(txs_logs));
        j_result["stateRoot"] = hex0x(state::mpt_hash(post_state));
    }
    else
        post_state = state;

    j_result["logsBloom"] = hex0x(bloom);
    j_result["receiptsRoot"] = hex0x(state::mpt_hash(receipts));
    if (rev >= EVMC_SHANGHAI)
        j_result["withdrawalsRoot"] = hex0x(state::mpt_hash(block.withdrawals));

    j_result["txRoot"] = hex0x(state::mpt_hash(transactions));
    j_result["gasUsed"] = hex0x(gas_used);
    if (rev >= EVMC_CANCUN)
    {
        j_result["blobGasUsed"] = hex0x(blob_gas_limit - blob_gas_left);
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
        *args.out_alloc << std::setw(2) << to_json(TestState{post_state});
    if (args.out_body != nullptr)
        *args.out_body << hex0x(rlp::encode(transactions));
}
}  // namespace evmone::tooling
