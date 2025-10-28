// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../state/block.hpp"
#include "../state/bloom_filter.hpp"
#include "../state/test_state.hpp"
#include "../state/transaction.hpp"
#include "../utils/blob_schedule.hpp"
#include "../utils/utils.hpp"
#include <evmc/evmc.hpp>
#include <span>
#include <vector>

namespace evmone::test
{
struct UnsupportedTestFeature : std::runtime_error
{
    using runtime_error::runtime_error;
};

// https://ethereum.org/en/developers/docs/blocks/
struct BlockHeader
{
    hash256 parent_hash;
    address coinbase;
    hash256 state_root;
    hash256 receipts_root;
    state::BloomFilter logs_bloom;
    int64_t difficulty;
    bytes32 prev_randao;
    int64_t block_number;
    int64_t gas_limit;
    int64_t gas_used;
    int64_t timestamp;
    bytes extra_data;
    uint64_t base_fee_per_gas;
    hash256 hash;
    hash256 transactions_root;
    hash256 withdrawal_root;
    hash256 parent_beacon_block_root;
    std::optional<uint64_t> blob_gas_used;
    std::optional<uint64_t> excess_blob_gas;
    hash256 requests_hash;
};

struct TestBlock
{
    state::BlockInfo block_info;
    std::vector<state::Transaction> transactions;
    size_t rlp_size = 0;
    bool withdrawals_parse_success = true;
    bool valid = true;

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

void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm);

}  // namespace evmone::test
