// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/errors.hpp>
#include <test/state/state.hpp>
#include <test/state/test_state.hpp>
#include <test/utils/blob_schedule.hpp>
#include <test/utils/utils.hpp>

using namespace evmc::literals;
using namespace evmone::state;
using namespace evmone::test;

TEST(state_tx, validate_nonce)
{
    const BlockInfo block{
        .gas_limit = 53000,
    };
    Transaction tx{
        .gas_limit = block.gas_limit,
        .sender = 0x02_address,
        .nonce = 1,
    };
    const TestState state{{tx.sender, {.nonce = 1, .balance = 1'000'000}}};

    ASSERT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_BERLIN, block.gas_limit, 0)));

    tx.nonce = 0;
    EXPECT_EQ(std::get<std::error_code>(
                  validate_transaction(state, block, tx, EVMC_BERLIN, block.gas_limit, 0))
                  .message(),
        "nonce too low");

    tx.nonce = 2;
    EXPECT_EQ(std::get<std::error_code>(
                  validate_transaction(state, block, tx, EVMC_BERLIN, block.gas_limit, 0))
                  .message(),
        "nonce too high");
}

TEST(state_tx, validate_sender)
{
    BlockInfo block{
        .gas_limit = 53000,
        .base_fee = 0,
    };
    Transaction tx{
        .gas_limit = block.gas_limit,
        .sender = 0x02_address,
    };
    const TestState state{{tx.sender, {}}};

    ASSERT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_BERLIN, block.gas_limit, 0)));

    block.base_fee = 1;

    EXPECT_EQ(std::get<std::error_code>(
                  validate_transaction(state, block, tx, EVMC_LONDON, block.gas_limit, 0))
                  .message(),
        "max fee per gas less than block base fee");

    tx.max_gas_price = block.base_fee;

    EXPECT_EQ(std::get<std::error_code>(
                  validate_transaction(state, block, tx, EVMC_LONDON, block.gas_limit, 0))
                  .message(),
        "insufficient funds for gas * price + value");
}

TEST(state_tx, validate_blob_tx)
{
    const BlockInfo block{
        .gas_limit = 1'000'000,
        .base_fee = 1,
        .blob_gas_used = 786432,
        .excess_blob_gas = 0,
        .blob_base_fee = 1,
    };
    Transaction tx{
        .type = Transaction::Type::blob,
        .gas_limit = 60000,
        .max_gas_price = block.base_fee,
        .sender = 0x02_address,
    };
    const TestState state{{tx.sender, {.balance = 1'000'000}}};

    const auto blob_gas_limit =
        static_cast<int64_t>(max_blob_gas_per_block(get_blob_params(EVMC_CANCUN)));
    EXPECT_EQ(std::get<std::error_code>(validate_transaction(
                  state, block, tx, EVMC_SHANGHAI, block.gas_limit, blob_gas_limit)),
        make_error_code(ErrorCode::TX_TYPE_NOT_SUPPORTED));

    EXPECT_EQ(std::get<std::error_code>(validate_transaction(state, block, tx, EVMC_CANCUN,
                                            block.gas_limit, blob_gas_limit))
                  .message(),
        make_error_code(ErrorCode::CREATE_BLOB_TX).message());

    tx.to = 0x01_address;
    EXPECT_EQ(std::get<std::error_code>(validate_transaction(
                  state, block, tx, EVMC_CANCUN, block.gas_limit, blob_gas_limit)),
        make_error_code(ErrorCode::EMPTY_BLOB_HASHES_LIST));

    for (uint8_t i = 0; i < 6; ++i)
    {
        auto h = 0x0100000000000000000000000000000000000000000000000000000000000000_bytes32;
        h.bytes[31] = i;
        tx.blob_hashes.emplace_back(h);
    }

    const auto expect_error = [&](int64_t g) {
        return std::get<std::error_code>(
            validate_transaction(state, block, tx, EVMC_CANCUN, block.gas_limit, g));
    };

    EXPECT_EQ(
        expect_error(blob_gas_limit), make_error_code(ErrorCode::BLOB_FEE_CAP_LESS_THAN_BLOCKS));

    tx.max_blob_gas_price = 1;
    tx.blob_hashes.push_back(
        0x0100000000000000000000000000000000000000000000000000000000000007_bytes32);
    EXPECT_EQ(expect_error(blob_gas_limit), make_error_code(ErrorCode::BLOB_GAS_LIMIT_EXCEEDED));

    tx.blob_hashes.pop_back();
    EXPECT_EQ(
        expect_error(blob_gas_limit - 1), make_error_code(ErrorCode::BLOB_GAS_LIMIT_EXCEEDED));

    EXPECT_EQ(std::get<TransactionProperties>(validate_transaction(state, block, tx, EVMC_CANCUN,
                                                  block.gas_limit, blob_gas_limit))
                  .execution_gas_limit,
        39000);

    tx.blob_hashes[0] = 0x0200000000000000000000000000000000000000000000000000000000000001_bytes32;
    EXPECT_EQ(expect_error(blob_gas_limit), make_error_code(ErrorCode::INVALID_BLOB_HASH_VERSION));
}

TEST(state_tx, validate_eof_create_transaction)
{
    const BlockInfo block{
        .gas_limit = 1'000'000,
    };
    const Transaction tx{
        .data = "EF00 01 010004 0200010001 030004 00 00000000 00 AABBCCDD"_hex,
        .gas_limit = 60000,
        .sender = 0x02_address,
        .to = {},
        .nonce = 1,
    };
    const TestState state{{tx.sender, {.nonce = 1, .balance = 1'000'000}}};

    EXPECT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_CANCUN, 60000, 0)));
    EXPECT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_PRAGUE, 60000, 0)));
    EXPECT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_EXPERIMENTAL, 60000, 0)));
}

TEST(state_tx, validate_tx_data_cost)
{
    // This test checks the transactions data cost calculation for different EVM revisions.

    const BlockInfo block{
        .gas_limit = 1'000'000,
        .base_fee = 1,
    };
    const Transaction tx{
        .data = "aa00bb00cc"_hex,
        .gas_limit = block.gas_limit,
        .max_gas_price = block.base_fee,
        .to = 0x00_address,
    };
    const TestState state{{tx.sender, {.balance = 1'000'000}}};

    const auto get_props = [&](evmc_revision rev) {
        const auto res = validate_transaction(state, block, tx, rev, block.gas_limit, 0);
        EXPECT_TRUE(holds_alternative<TransactionProperties>(res));
        if (holds_alternative<TransactionProperties>(res))
            return get<TransactionProperties>(res);
        return TransactionProperties{};
    };
    const auto from_data_cost = [&](int64_t nonzero_cost, int64_t zero_cost) {
        // The counts for the zero/nonzero bytes are taken from the test tx.data input.
        return tx.gas_limit - (21000 + 3 * nonzero_cost + 2 * zero_cost);
    };

    EXPECT_EQ(get_props(EVMC_PETERSBURG).execution_gas_limit, from_data_cost(68, 4));
    EXPECT_EQ(get_props(EVMC_ISTANBUL).execution_gas_limit, from_data_cost(16, 4));
    EXPECT_EQ(get_props(EVMC_CANCUN).execution_gas_limit, from_data_cost(16, 4));
    EXPECT_EQ(get_props(EVMC_PRAGUE).execution_gas_limit, from_data_cost(16, 4));

    EXPECT_EQ(get_props(EVMC_PETERSBURG).min_gas_cost, 0);
    EXPECT_EQ(get_props(EVMC_ISTANBUL).min_gas_cost, 0);
    EXPECT_EQ(get_props(EVMC_CANCUN).min_gas_cost, 0);
    EXPECT_EQ(get_props(EVMC_PRAGUE).min_gas_cost, 21000 + (4 * 3 + 2) * 10);
}

TEST(state_tx, max_blob_count)
{
    const BlockInfo block{
        .gas_limit = 1'000'000,
        .base_fee = 1,
        .blob_gas_used = 786432,
        .excess_blob_gas = 0,
        .blob_base_fee = 1,
    };
    Transaction tx{
        .type = Transaction::Type::blob,
        .gas_limit = 60000,
        .max_gas_price = block.base_fee,
        .max_blob_gas_price = 1,
        .sender = 0x02_address,
        .to = 0x01_address,
    };
    const TestState state{{tx.sender, {.balance = 1'000'000}}};
    const auto blob_gas_limit =
        static_cast<int64_t>(max_blob_gas_per_block(get_blob_params(EVMC_CANCUN)));

    // Add MAX_TX_BLOB_COUNT blobs
    for (size_t i = 0; i < MAX_TX_BLOB_COUNT; ++i)
    {
        auto h = 0x0100000000000000000000000000000000000000000000000000000000000000_bytes32;
        h.bytes[31] = static_cast<uint8_t>(i);
        tx.blob_hashes.emplace_back(h);
    }

    // Should be valid
    EXPECT_FALSE(holds_alternative<std::error_code>(
        validate_transaction(state, block, tx, EVMC_CANCUN, block.gas_limit, blob_gas_limit)));

    // Add one more blob to exceed the limit
    tx.blob_hashes.emplace_back(
        0x01000000000000000000000000000000000000000000000000000000000000FF_bytes32);

    EXPECT_EQ(std::get<std::error_code>(validate_transaction(
                  state, block, tx, EVMC_CANCUN, block.gas_limit, blob_gas_limit)),
        make_error_code(ErrorCode::BLOB_GAS_LIMIT_EXCEEDED));
}

TEST(state_tx, max_gas_limit_exceeded)
{
    const BlockInfo block{.gas_limit = MAX_TX_GAS_LIMIT + 1};
    const Transaction tx{.gas_limit = block.gas_limit};
    const TestState state;

    EXPECT_EQ(std::get<std::error_code>(
                  validate_transaction(state, block, tx, EVMC_OSAKA, block.gas_limit, 0)),
        make_error_code(ErrorCode::MAX_GAS_LIMIT_EXCEEDED));
}
