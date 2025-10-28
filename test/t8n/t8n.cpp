// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../state/errors.hpp"
#include "../state/ethash_difficulty.hpp"
#include "../state/mpt_hash.hpp"
#include "../state/requests.hpp"
#include "../state/rlp.hpp"
#include "../statetest/statetest.hpp"
#include "../utils/utils.hpp"
#include <evmone/evmone.h>
#include <evmone/version.h>
#include <evmone/vm.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace fs = std::filesystem;
namespace json = nlohmann;
using namespace evmone;
using namespace evmone::test;
using namespace std::literals;

int main(int argc, const char* argv[])
{
    evmc_revision rev = {};
    fs::path alloc_file;
    fs::path env_file;
    fs::path txs_file;
    fs::path blob_params_file;
    fs::path output_dir;
    fs::path output_result_file;
    fs::path output_alloc_file;
    fs::path output_body_file;
    std::optional<uint64_t> block_reward;
    uint64_t chain_id = 0;
    bool trace = false;
    fs::path opcode_count_filename;
    std::string opcode_count_file;
    bool pre_state_only = false;

    try
    {
        for (int i = 0; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};

            if (arg == "-v" || arg == "--version")
            {
                std::cout << "evmone-t8n " EVMONE_VERSION "\n";
                return 0;
            }
            if (arg == "--state.fork" && ++i < argc)
                rev = to_rev(argv[i]);
            else if (arg == "--input.alloc" && ++i < argc)
                alloc_file = argv[i];
            else if (arg == "--input.env" && ++i < argc)
                env_file = argv[i];
            else if (arg == "--input.txs" && ++i < argc)
                txs_file = argv[i];
            else if (arg == "--input.blobParams" && ++i < argc)
                blob_params_file = argv[i];
            else if (arg == "--output.basedir" && ++i < argc)
            {
                output_dir = argv[i];
                fs::create_directories(output_dir);
            }
            else if (arg == "--output.result" && ++i < argc)
                output_result_file = argv[i];
            else if (arg == "--output.alloc" && ++i < argc)
                output_alloc_file = argv[i];
            else if (arg == "--state.chainid" && ++i < argc)
                chain_id = intx::from_string<uint64_t>(argv[i]);
            else if (arg == "--output.body" && ++i < argc)
                output_body_file = argv[i];
            else if (arg == "--trace")
                trace = true;
            else if (arg == "--opcode.count" && ++i < argc)
                opcode_count_filename = argv[i];
            else if (arg == "--state.reward" && ++i < argc)
            {
                if (argv[i] == "-1"sv)  // Hack to compute the root hash of the pre-state.
                    pre_state_only = true;
                else
                    block_reward = intx::from_string<uint64_t>(argv[i]);
            }
        }

        state::BlockInfo block;
        TestBlockHashes block_hashes;
        TestState state;

        state::BlobParams blob_params;

        if (!blob_params_file.empty())
        {
            const auto j = json::json::parse(std::ifstream{blob_params_file}, nullptr, false);
            blob_params = from_json<state::BlobParams>(j);
        }
        else
        {
            // Use hardcoded blob schedule if no blob config file is provided.
            blob_params = get_blob_params(rev);
        }

        if (!alloc_file.empty())
        {
            const auto j = json::json::parse(std::ifstream{alloc_file}, nullptr, false);
            state = from_json<TestState>(j);
            validate_state(state, rev);
        }
        if (!env_file.empty())
        {
            const auto j = json::json::parse(std::ifstream{env_file});
            block = from_json_with_rev(j, rev, blob_params);
            block_hashes = from_json<TestBlockHashes>(j);
        }

        json::json j_result;

        // Difficulty was received from upstream. No need to calc
        // TODO: Check if it's needed by the blockchain test. If not remove if statement true branch
        if (block.difficulty != 0)
            j_result["currentDifficulty"] = hex0x(block.difficulty);
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
        if (!txs_file.empty())
        {
            const auto j_txs = json::json::parse(std::ifstream{txs_file});

            evmc::VM vm{evmc_create_evmone()};

            if (trace)
                vm.set_option("trace", "1");
            if (!opcode_count_filename.empty())
            {
                opcode_count_file = (output_dir / opcode_count_filename).string();
                vm.set_option("opcode.count", opcode_count_file.c_str());
            }

            std::vector<state::Log> txs_logs;

            if (j_txs.is_array())
            {
                j_result["receipts"] = json::json::array();
                j_result["rejected"] = json::json::array();

                if (!pre_state_only)
                    test::system_call_block_start(state, block, block_hashes, rev, vm);

                for (size_t i = 0; i < j_txs.size(); ++i)
                {
                    auto tx = test::from_json<state::Transaction>(j_txs[i]);
                    tx.chain_id = chain_id;

                    const auto computed_tx_hash = keccak256(rlp::encode(tx));
                    const auto computed_tx_hash_str = hex0x(computed_tx_hash);

                    if (j_txs[i].contains("hash"))
                    {
                        const auto loaded_tx_hash_opt =
                            evmc::from_hex<bytes32>(j_txs[i]["hash"].get<std::string>());

                        if (loaded_tx_hash_opt != computed_tx_hash)
                            throw std::logic_error("transaction hash mismatched: computed " +
                                                   computed_tx_hash_str + ", expected " +
                                                   hex0x(loaded_tx_hash_opt.value()));
                    }

                    std::ofstream trace_file_output;
                    const auto orig_clog_buf = std::clog.rdbuf();
                    if (trace)
                    {
                        const auto output_filename =
                            output_dir /
                            ("trace-" + std::to_string(i) + "-" + computed_tx_hash_str + ".jsonl");

                        // `trace` flag enables trace logging to std::clog.
                        // Redirect std::clog to the output file.
                        trace_file_output.open(output_filename);
                        std::clog.rdbuf(trace_file_output.rdbuf());
                    }

                    auto res = test::transition(
                        state, block, block_hashes, tx, rev, vm, block_gas_left, blob_gas_left);

                    if (holds_alternative<std::error_code>(res))
                    {
                        const auto ec = std::get<std::error_code>(res);
                        json::json j_rejected_tx;
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
                        j_receipt["logs"] = json::json::array();  // FIXME: Add to_json<state:Log>
                        j_receipt["root"] = "";
                        j_receipt["status"] = "0x1";
                        j_receipt["transactionIndex"] = hex0x(i);
                        blob_gas_left -= static_cast<int64_t>(tx.blob_gas_used());
                        transactions.emplace_back(std::move(tx));
                        block_gas_left -= receipt.gas_used;
                        receipts.emplace_back(std::move(receipt));
                    }

                    // Restore original std::clog buffer (otherwise std::clog crashes at exit).
                    if (trace)
                        std::clog.rdbuf(orig_clog_buf);
                }
            }

            if (!pre_state_only && rev >= EVMC_PRAGUE)
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

            test::finalize(
                state, rev, block.coinbase, block_reward, block.ommers, block.withdrawals);

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
            j_result["blobGasUsed"] = hex0x(
                static_cast<int64_t>(state::max_blob_gas_per_block(blob_params)) - blob_gas_left);
            if (block.excess_blob_gas.has_value())
                j_result["currentExcessBlobGas"] = hex0x(*block.excess_blob_gas);
        }
        if (rev >= EVMC_PRAGUE)
        {
            // EIP-7685: General purpose execution layer requests
            j_result["requests"] = json::json::array();
            for (const auto& r : requests)
            {
                if (!r.data().empty())
                    // Only report non-empty requests. Include the leading type byte.
                    j_result["requests"].emplace_back(hex0x(r.raw_data));
            }

            auto requests_hash = calculate_requests_hash(requests);

            j_result["requestsHash"] = hex0x(requests_hash);
        }

        std::ofstream{output_dir / output_result_file} << std::setw(2) << j_result;

        // Print out current state to outAlloc file
        std::ofstream{output_dir / output_alloc_file} << std::setw(2) << to_json(TestState{state});

        if (!output_body_file.empty())
            std::ofstream{output_dir / output_body_file} << hex0x(rlp::encode(transactions));
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
