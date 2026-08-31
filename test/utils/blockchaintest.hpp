// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>
#include <nlohmann/json.hpp>
#include <test/state/block.hpp>
#include <test/state/bloom_filter.hpp>
#include <test/state/transaction.hpp>
#include <test/utils/blob_schedule.hpp>
#include <test/utils/test_report.hpp>
#include <test/utils/test_state.hpp>
#include <test/utils/utils.hpp>
#include <vector>

namespace json = nlohmann;

namespace evmone::test
{
// https://ethereum.org/en/developers/docs/blocks/
struct BlockHeader
{
    hash256 parent_hash;
    address coinbase;
    hash256 state_root;
    hash256 receipts_root;
    state::BloomFilter logs_bloom;
    int64_t difficulty = 0;
    bytes32 prev_randao;
    int64_t block_number = 0;
    int64_t gas_limit = 0;
    int64_t gas_used = 0;
    int64_t timestamp = 0;
    bytes extra_data;
    uint64_t base_fee_per_gas = 0;
    hash256 hash;
    hash256 transactions_root;
    hash256 withdrawal_root;
    hash256 parent_beacon_block_root;
    std::optional<uint64_t> blob_gas_used;
    std::optional<uint64_t> excess_blob_gas;
    hash256 requests_hash;
    std::optional<uint64_t> slot_number;  ///< EIP-7843 — absent before Amsterdam.
};

struct TestBlock
{
    state::BlockInfo block_info;
    std::vector<state::Transaction> transactions;
    bytes rlp;  ///< The block's complete serialization.
    bool withdrawals_parse_success = true;
    std::string expected_exception;  ///< Empty for valid blocks.

    BlockHeader expected_block_header;
};

struct BlockchainTest
{
    struct Expectation
    {
        hash256 last_block_hash;
        std::variant<TestState, hash256> post_state;
    };

    std::string name;

    std::vector<TestBlock> test_blocks;
    BlockHeader genesis_block_header;
    TestState pre_state;
    RevisionSchedule rev;
    std::string network;
    BlobSchedule blob_schedule;

    Expectation expectation;
};

std::vector<BlockchainTest> load_blockchain_tests(std::istream& input);

/// Builds the test named @p name in a fixture file from its JSON value @p j.
BlockchainTest make_blockchain_test(const std::string& name, const json::json& j);

/// Execute the blockchain @p tests using the @p vm, recording what does not match into @p report.
void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm, TestReport& report);
}  // namespace evmone::test
