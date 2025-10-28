// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/state.hpp>
#include <test/utils/blob_schedule.hpp>

using namespace evmone::state;
using namespace evmone::test;
using namespace intx::literals;

TEST(state_block, blob_gas_price)
{
    static constexpr uint64_t TARGET_BLOB_GAS_PER_BLOCK_CANCUN = 0x60000;

    auto blob_params = get_blob_params(EVMC_CANCUN);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 0), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 1), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_CANCUN), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_CANCUN * 2), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_CANCUN * 7), 2);

    EXPECT_EQ(compute_blob_gas_price(blob_params, 10'000'000), 19);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 100'000'000), 10203769476395);

    // Close to the computation overflowing:
    EXPECT_EQ(compute_blob_gas_price(blob_params, 400'000'000),
        10840331274704280429132033759016842817414750029778539_u256);
}

TEST(state_block, blob_gas_price_prague)
{
    static constexpr uint64_t TARGET_BLOB_GAS_PER_BLOCK_PRAGUE = 0xc0000;

    auto blob_params = get_blob_params(EVMC_PRAGUE);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 0), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 1), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_PRAGUE), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_PRAGUE * 2), 1);
    EXPECT_EQ(compute_blob_gas_price(blob_params, TARGET_BLOB_GAS_PER_BLOCK_PRAGUE * 7), 3);

    EXPECT_EQ(compute_blob_gas_price(blob_params, 10'000'000), 7);
    EXPECT_EQ(compute_blob_gas_price(blob_params, 100'000'000), 470442149);

    // Close to the computation overflowing:
    EXPECT_EQ(
        compute_blob_gas_price(blob_params, 400'000'000), 48980690787953896757236758600209812_u256);
}
