// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../state/ethash_difficulty.hpp"
#include "../state/mpt_hash.hpp"
#include "../state/requests.hpp"
#include "../state/rlp.hpp"
#include "../test/statetest/statetest.hpp"
#include "blockchaintest.hpp"
#include <gtest/gtest.h>

namespace evmone::test
{

/// The CL gossip protocol constraint of the maximum block size (EIP-7934).
constexpr size_t MAX_BLOCK_SIZE = 10 * 1024 * 1024;
/// The safety margin for beacon block content (EIP-7934).
constexpr size_t SAFETY_MARGIN = 2 * 1024 * 1024;
/// The maximum EL block size when RLP encoded (EIP-7934).
constexpr size_t MAX_RLP_BLOCK_SIZE = MAX_BLOCK_SIZE - SAFETY_MARGIN;

struct RejectedTransaction
{
    hash256 hash;
    size_t index;
    std::string message;
};

struct TransitionResult
{
    std::vector<state::TransactionReceipt> receipts;
    std::vector<RejectedTransaction> rejected;
    std::optional<std::vector<state::Requests>> requests;
    int64_t gas_used;
    state::BloomFilter bloom;
    int64_t blob_gas_left;
    TestState block_state;
};

namespace
{
TransitionResult apply_block(const TestState& state, evmc::VM& vm, const state::BlockInfo& block,
    const state::BlockHashes& block_hashes, const std::vector<state::Transaction>& txs,
    evmc_revision rev, std::optional<int64_t> block_reward)
{
    TestState block_state(state);
    system_call_block_start(block_state, block, block_hashes, rev, vm);

    std::vector<state::Log> txs_logs;
    int64_t block_gas_left = block.gas_limit;
    auto blob_gas_left = static_cast<int64_t>(block.blob_gas_used.value_or(0));

    std::vector<RejectedTransaction> rejected_txs;
    std::vector<state::TransactionReceipt> receipts;

    int64_t cumulative_gas_used = 0;

    for (size_t i = 0; i < txs.size(); ++i)
    {
        const auto& tx = txs[i];

        const auto computed_tx_hash = keccak256(rlp::encode(tx));
        auto res = test::transition(
            block_state, block, block_hashes, tx, rev, vm, block_gas_left, blob_gas_left);

        if (holds_alternative<std::error_code>(res))
        {
            const auto ec = std::get<std::error_code>(res);
            rejected_txs.push_back({computed_tx_hash, i, ec.message()});
        }
        else
        {
            auto& receipt = get<state::TransactionReceipt>(res);

            const auto& tx_logs = receipt.logs;

            txs_logs.insert(txs_logs.end(), tx_logs.begin(), tx_logs.end());
            cumulative_gas_used += receipt.gas_used;
            receipt.cumulative_gas_used = cumulative_gas_used;
            if (rev < EVMC_BYZANTIUM)
                receipt.post_state = state::mpt_hash(block_state);

            block_gas_left -= receipt.gas_used;
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

        auto requests_result = system_call_block_end(block_state, block, block_hashes, rev, vm);
        if (!requests_result.has_value())
            return std::nullopt;
        std::ranges::move(*requests_result, std::back_inserter(collected));

        return collected;
    }();

    finalize(block_state, rev, block.coinbase, block_reward, block.ommers, block.withdrawals);

    const auto bloom = compute_bloom_filter(receipts);

    return {std::move(receipts), std::move(rejected_txs), std::move(requests), cumulative_gas_used,
        bloom, blob_gas_left, std::move(block_state)};
}

bool validate_block(evmc_revision rev, state::BlobParams blob_params, const TestBlock& test_block,
    const BlockHeader* parent_header, bool parent_has_ommers) noexcept
{
    // NOTE: includes only block validity unrelated to individual txs. See `apply_block`.

    // Fail if parent header was not found.
    if (parent_header == nullptr)
        return false;

    if (test_block.block_info.number != parent_header->block_number + 1)
        return false;

    if (test_block.block_info.gas_used > test_block.block_info.gas_limit)
        return false;

    // Some tests have gas limit at INT64_MAX, so we cast to uint64_t to avoid overflow.
    const auto parent_header_gas_limit_u64 = static_cast<uint64_t>(parent_header->gas_limit);
    const auto test_block_gas_limit_u64 = static_cast<uint64_t>(test_block.block_info.gas_limit);
    if (test_block_gas_limit_u64 >=
        parent_header_gas_limit_u64 + parent_header_gas_limit_u64 / 1024)
        return false;
    if (test_block_gas_limit_u64 <=
        parent_header_gas_limit_u64 - parent_header_gas_limit_u64 / 1024)
        return false;

    // Block gas limit minimum from Yellow Paper.
    if (test_block.block_info.gas_limit < 5000)
        return false;

    // FIXME: Some tests have timestamp not fitting into int64_t, type has to be uint64_t.
    if (static_cast<uint64_t>(test_block.block_info.timestamp) <=
        static_cast<uint64_t>(parent_header->timestamp))
        return false;

    if (test_block.block_info.difficulty != state::calculate_difficulty(parent_header->difficulty,
                                                parent_has_ommers, parent_header->timestamp,
                                                test_block.block_info.timestamp,
                                                test_block.block_info.number, rev))
        return false;

    if (rev >= EVMC_PARIS && !test_block.block_info.ommers.empty())
        return false;


    for (const auto& ommer : test_block.block_info.ommers)
    {
        // Check that ommer block number difference with current block is within allowed range.
        // https://github.com/ethereum/execution-specs/blob/ee73be5c4d83a2e3c358bd14990878002e52ba9e/src/ethereum/gray_glacier/fork.py#L623
        if (ommer.delta < 1 || ommer.delta > 6)
            return false;
    }

    if (test_block.block_info.extra_data.size() > 32)
        return false;

    if (rev >= EVMC_LONDON)
    {
        const auto calculated_base_fee = state::calc_base_fee(
            parent_header->gas_limit, parent_header->gas_used, parent_header->base_fee_per_gas);
        if (test_block.block_info.base_fee != calculated_base_fee)
            return false;
    }

    if (rev >= EVMC_CANCUN)
    {
        // `excess_blob_gas` and `blob_gas_used` mandatory after Cancun and invalid before.
        if (!test_block.block_info.excess_blob_gas.has_value() ||
            !test_block.block_info.blob_gas_used.has_value())
            return false;

        // Check that the excess blob gas was updated correctly.
        // According to EIP-7918 current blocks params (`rev`) should be used for parent base fee
        // calculation.
        const auto parent_blob_base_fee =
            state::compute_blob_gas_price(blob_params, parent_header->excess_blob_gas.value_or(0));
        if (*test_block.block_info.excess_blob_gas !=
            state::calc_excess_blob_gas(rev, blob_params, parent_header->blob_gas_used.value_or(0),
                parent_header->excess_blob_gas.value_or(0), parent_header->base_fee_per_gas,
                parent_blob_base_fee))
            return false;

        // Ensure the total blob gas spent is at most equal to the limit
        if (*test_block.block_info.blob_gas_used > state::max_blob_gas_per_block(blob_params))
            return false;
    }
    else
    {
        if (test_block.block_info.excess_blob_gas.has_value() ||
            test_block.block_info.blob_gas_used.has_value())
            return false;
    }

    // Block is invalid if some of the withdrawal fields failed to be parsed.
    if (!test_block.withdrawals_parse_success)
        return false;

    if (rev >= EVMC_OSAKA && test_block.rlp_size > MAX_RLP_BLOCK_SIZE)
        return false;

    return true;
}

std::optional<int64_t> mining_reward(evmc_revision rev) noexcept
{
    if (rev < EVMC_BYZANTIUM)
        return 5'000000000'000000000;
    if (rev < EVMC_CONSTANTINOPLE)
        return 3'000000000'000000000;
    if (rev < EVMC_PARIS)
        return 2'000000000'000000000;
    return std::nullopt;
}

std::string print_state(const TestState& s)
{
    std::stringstream out;

    for (const auto& [key, acc] : s)
    {
        out << key << " : \n";
        out << "\tnonce : " << acc.nonce << "\n";
        out << "\tbalance : " << hex0x(acc.balance) << "\n";
        out << "\tcode : " << hex0x(acc.code) << "\n";

        if (!acc.storage.empty())
        {
            out << "\tstorage : \n";
            for (const auto& [s_key, val] : acc.storage)
            {
                if (!is_zero(val))  // Skip 0 values.
                    out << "\t\t" << s_key << " : " << hex0x(val) << "\n";
            }
        }
    }

    return out.str();
}
}  // namespace

void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm)
{
    for (size_t case_index = 0; case_index != tests.size(); ++case_index)
    {
        const auto& c = tests[case_index];
        const auto rev_schedule = to_rev_schedule(c.network);
        SCOPED_TRACE(std::string{evmc::to_string(rev_schedule.get_revision(0))} + '/' +
                     std::to_string(case_index) + '/' + c.name);

        // Validate the genesis block header.
        EXPECT_EQ(c.genesis_block_header.block_number, 0);
        EXPECT_EQ(c.genesis_block_header.gas_used, 0);
        EXPECT_EQ(c.genesis_block_header.transactions_root, state::EMPTY_MPT_HASH);
        EXPECT_EQ(c.genesis_block_header.receipts_root, state::EMPTY_MPT_HASH);
        EXPECT_EQ(c.genesis_block_header.withdrawal_root,
            rev_schedule.get_revision(c.genesis_block_header.timestamp) >= EVMC_SHANGHAI ?
                state::EMPTY_MPT_HASH :
                bytes32{});
        EXPECT_EQ(c.genesis_block_header.logs_bloom, bytes_view{state::BloomFilter{}});

        TestBlockHashes block_hashes{
            {c.genesis_block_header.block_number, c.genesis_block_header.hash}};

        struct BlockData
        {
            const BlockHeader* header;
            bool has_ommers = false;
            TestState post_state;
            intx::uint256 total_difficulty;
        };
        std::unordered_map<hash256, BlockData> block_data{{{c.genesis_block_header.hash,
            {&c.genesis_block_header, false, c.pre_state, c.genesis_block_header.difficulty}}}};
        const auto* canonical_state = &c.pre_state;
        intx::uint256 max_total_difficulty = c.genesis_block_header.difficulty;

        for (size_t i = 0; i < c.test_blocks.size(); ++i)
        {
            const auto& test_block = c.test_blocks[i];
            const auto& bi = test_block.block_info;

            const auto parent_data_it = block_data.find(test_block.block_info.parent_hash);
            const auto* parent_header =
                parent_data_it != block_data.end() ? parent_data_it->second.header : nullptr;
            const auto parent_has_ommers =
                parent_data_it != block_data.end() && parent_data_it->second.has_ommers;

            const auto rev = rev_schedule.get_revision(bi.timestamp);
            const auto blob_params = get_blob_params(c.network, c.blob_schedule, bi.timestamp);

            SCOPED_TRACE(std::string{evmc::to_string(rev)} + '/' + std::to_string(case_index) +
                         '/' + c.name + '/' + std::to_string(test_block.block_info.number));

            if (test_block.valid)
            {
                ASSERT_TRUE(
                    validate_block(rev, blob_params, test_block, parent_header, parent_has_ommers))
                    << "Expected block to be valid (validate_block)";

                // Block being valid guarantees its parent was found.
                assert(parent_data_it != block_data.end());
                const auto& pre_state = parent_data_it->second.post_state;

                auto res = apply_block(pre_state, vm, bi, block_hashes, test_block.transactions,
                    rev, mining_reward(rev));

                ASSERT_TRUE(res.requests.has_value());

                block_hashes[test_block.expected_block_header.block_number] =
                    test_block.expected_block_header.hash;
                const auto [inserted_it, _] = block_data.insert({test_block.block_info.hash,
                    {
                        .header = &test_block.expected_block_header,
                        .has_ommers = !test_block.block_info.ommers.empty(),
                        .post_state = std::move(res.block_state),
                        .total_difficulty = parent_data_it->second.total_difficulty +
                                            test_block.block_info.difficulty,
                    }});
                if (inserted_it->second.total_difficulty >= max_total_difficulty)
                {
                    canonical_state = &inserted_it->second.post_state;
                    max_total_difficulty = inserted_it->second.total_difficulty;
                }

                EXPECT_TRUE(res.rejected.empty())
                    << "Invalid transaction in block expected to be valid";
                EXPECT_TRUE(res.blob_gas_left == 0)
                    << "Transactions used more or less blob gas than expected in block header";

                EXPECT_EQ(state::mpt_hash(inserted_it->second.post_state),
                    test_block.expected_block_header.state_root);

                if (rev >= EVMC_SHANGHAI)
                {
                    EXPECT_EQ(state::mpt_hash(test_block.block_info.withdrawals),
                        test_block.expected_block_header.withdrawal_root);
                }

                EXPECT_EQ(state::mpt_hash(test_block.transactions),
                    test_block.expected_block_header.transactions_root);
                EXPECT_EQ(
                    state::mpt_hash(res.receipts), test_block.expected_block_header.receipts_root);
                if (rev >= EVMC_PRAGUE)
                {
                    EXPECT_EQ(calculate_requests_hash(*res.requests),
                        test_block.expected_block_header.requests_hash);
                }
                EXPECT_EQ(res.gas_used, test_block.expected_block_header.gas_used);
                EXPECT_EQ(
                    bytes_view{res.bloom}, bytes_view{test_block.expected_block_header.logs_bloom});
            }
            else
            {
                if (!validate_block(rev, blob_params, test_block, parent_header, parent_has_ommers))
                    continue;

                // Block being valid guarantees its parent was found.
                assert(parent_data_it != block_data.end());
                const auto& pre_state = parent_data_it->second.post_state;

                const auto res = apply_block(pre_state, vm, bi, block_hashes,
                    test_block.transactions, rev, mining_reward(rev));
                if (!res.requests.has_value())
                    continue;
                if (!res.rejected.empty())
                    continue;
                if (res.blob_gas_left != 0)
                    continue;

                if (state::mpt_hash(res.block_state) != test_block.expected_block_header.state_root)
                    continue;

                if (rev >= EVMC_SHANGHAI && state::mpt_hash(test_block.block_info.withdrawals) !=
                                                test_block.expected_block_header.withdrawal_root)
                    continue;
                if (state::mpt_hash(test_block.transactions) !=
                    test_block.expected_block_header.transactions_root)
                    continue;
                if (state::mpt_hash(res.receipts) != test_block.expected_block_header.receipts_root)
                    continue;
                if (rev >= EVMC_PRAGUE && calculate_requests_hash(*res.requests) !=
                                              test_block.expected_block_header.requests_hash)
                    continue;
                if (res.gas_used != test_block.expected_block_header.gas_used)
                    continue;
                if (bytes_view{res.bloom} !=
                    bytes_view{test_block.expected_block_header.logs_bloom})
                    continue;

                EXPECT_TRUE(false) << "Expected block to be invalid but resulted valid";
            }
        }
        const auto expected_post_hash =
            std::holds_alternative<TestState>(c.expectation.post_state) ?
                state::mpt_hash(std::get<TestState>(c.expectation.post_state)) :
                std::get<hash256>(c.expectation.post_state);
        EXPECT_EQ(state::mpt_hash(*canonical_state), expected_post_hash)
            << "Result state:\n"
            << print_state(*canonical_state)
            << (std::holds_alternative<TestState>(c.expectation.post_state) ?
                       "\n\nExpected state:\n" +
                           print_state(std::get<TestState>(c.expectation.post_state)) :
                       "");
    }
}

}  // namespace evmone::test
