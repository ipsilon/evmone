// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/statetest/statetest.hpp>

using namespace evmone;

TEST(statetest_loader, block_info)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0x00",
            "withdrawals": []
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
}

TEST(statetest_loader, block_info_hex)
{
    constexpr std::string_view input = R"({
        "currentCoinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "currentGasLimit": "0x16345785D8A0000",
        "currentNumber": "1",
        "currentTimestamp": "0x3E8",
        "currentRandom": "0x00",
        "currentDifficulty": "1",
        "parentDifficulty": "0",
        "parentBaseFee": "7",
        "parentGasUsed": "0",
        "parentGasLimit": "0x16345785D8A0000",
        "parentTimstamp": "0",
        "ommers": [],
        "withdrawals": [],
        "parentUncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 100000000000000000);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 1000);
    EXPECT_EQ(bi.number, 1);
}

TEST(statetest_loader, block_info_dec)
{
    constexpr std::string_view input = R"({
        "currentCoinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "currentGasLimit": "100000000000000000",
        "currentNumber": "1",
        "currentTimestamp": "1000",
        "currentRandom": "0x00",
        "currentDifficulty": "0",
        "parentDifficulty": "0",
        "parentBaseFee": "7",
        "parentGasUsed": "0",
        "parentGasLimit": "100000000000000000",
        "parentTimstamp": "0",
        "ommers": [],
        "withdrawals": [],
        "parentUncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 100000000000000000);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 1000);
    EXPECT_EQ(bi.number, 1);
}

TEST(statetest_loader, block_info_0_current_difficulty)
{
    constexpr std::string_view input = R"({
        "currentCoinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "currentGasLimit": "100000000000000000",
        "currentNumber": "1",
        "currentTimestamp": "1000",
        "currentDifficulty": "0",
        "parentBaseFee": "7",
        "parentGasUsed": "0",
        "parentGasLimit": "100000000000000000",
        "parentTimstamp": "0",
        "ommers": [],
        "withdrawals": [],
        "parentUncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 100000000000000000);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 1000);
    EXPECT_EQ(bi.number, 1);
}

TEST(statetest_loader, block_info_0_parent_difficulty)
{
    constexpr std::string_view input = R"({
        "currentCoinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
        "currentGasLimit": "100000000000000000",
        "currentNumber": "1",
        "currentTimestamp": "1000",
        "parentDifficulty": "0x0",
        "parentBaseFee": "7",
        "parentGasUsed": "0",
        "parentGasLimit": "100000000000000000",
        "parentTimestamp": "253",
        "ommers": [],
        "withdrawals": [],
        "parentUncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
    })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 100000000000000000);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 1000);
    EXPECT_EQ(bi.number, 1);
    EXPECT_EQ(bi.parent_timestamp, 253);
}

TEST(statetest_loader, block_info_0_random)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0",
            "withdrawals": []
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
}

TEST(statetest_loader, block_info_withdrawals)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0x00",
            "withdrawals": [
                {
                    "index": "0x0",
                    "validatorIndex": "0x0",
                    "address": "0x0000000000000000000000000000000000000100",
                    "amount": "0x800000000"
                },
                {
                    "index": "0x1",
                    "validatorIndex": "0x1",
                    "address": "0x0000000000000000000000000000000000000200",
                    "amount": "0xffffffffffffffff"
                }
            ]
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
    const auto& withdrawals = bi.withdrawals;
    EXPECT_EQ(withdrawals.size(), 2);
    EXPECT_EQ(withdrawals[0].recipient, 0x0000000000000000000000000000000000000100_address);
    EXPECT_EQ(withdrawals[0].get_amount(), intx::uint256{0x800000000} * 1'000'000'000);
    EXPECT_EQ(withdrawals[1].recipient, 0x0000000000000000000000000000000000000200_address);
    EXPECT_EQ(withdrawals[1].get_amount(), intx::uint256{0xffffffffffffffff} * 1'000'000'000);
}

TEST(statetest_loader, block_info_ommers)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0x00",
            "ommers": [
                {
                    "address": "0x0000000000000000000000000000000000000100",
                    "delta": 1
                },
                {
                    "address": "0x0000000000000000000000000000000000000200",
                    "delta": 4
                }
            ],
            "withdrawals": []
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
    EXPECT_EQ(bi.withdrawals.size(), 0);
    EXPECT_EQ(bi.ommers.size(), 2);
    EXPECT_EQ(bi.ommers[0].beneficiary, 0x0000000000000000000000000000000000000100_address);
    EXPECT_EQ(bi.ommers[0].delta, 1);
    EXPECT_EQ(bi.ommers[1].beneficiary, 0x0000000000000000000000000000000000000200_address);
    EXPECT_EQ(bi.ommers[1].delta, 4);
}

TEST(statetest_loader, block_info_parent_blob_gas)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0x00",
            "withdrawals": [],
            "parentBaseFee": "7",
            "parentExcessBlobGas": "1",
            "parentBlobGasUsed": "0x60000"
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
    EXPECT_EQ(bi.excess_blob_gas, 1);
}

TEST(statetest_loader, block_info_current_blob_gas)
{
    constexpr std::string_view input = R"({
            "currentCoinbase": "0x1111111111111111111111111111111111111111",
            "currentDifficulty": "0x0",
            "currentGasLimit": "0x0",
            "currentNumber": "0",
            "currentTimestamp": "0",
            "currentBaseFee": "7",
            "currentRandom": "0x00",
            "withdrawals": [],
            "currentExcessBlobGas": "2"
        })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.coinbase, 0x1111111111111111111111111111111111111111_address);
    EXPECT_EQ(bi.prev_randao, 0x00_bytes32);
    EXPECT_EQ(bi.gas_limit, 0x0);
    EXPECT_EQ(bi.base_fee, 7);
    EXPECT_EQ(bi.timestamp, 0);
    EXPECT_EQ(bi.number, 0);
    EXPECT_EQ(bi.excess_blob_gas, 2);
}

TEST(statetest_loader, block_info_parent_beacon_block_root)
{
    constexpr std::string_view input = R"({
        "currentNumber": "0",
        "currentTimestamp": "0",
        "currentGasLimit": "0",
        "currentCoinbase": "",
        "parentBeaconBlockRoot": "0xbeac045007"
    })";

    const auto blob_params = test::get_blob_params(EVMC_CANCUN);
    const auto bi = test::from_json_with_rev(json::json::parse(input), EVMC_CANCUN, blob_params);
    EXPECT_EQ(bi.parent_beacon_block_root, 0xbeac045007_bytes32);
}

TEST(statetest_loader, block_hashes)
{
    constexpr std::string_view input = R"({
            "blockHashes": {
                "0" : "0xe729de3fec21e30bea3d56adb01ed14bc107273c2775f9355afb10f594a10d9e",
                "1" : "0xb5eee60b45801179cbde3781b9a5dee9b3111554618c9cda3d6f7e351fd41e0b"
            }})";

    const auto bh = test::from_json<test::TestBlockHashes>(json::json::parse(input));
    EXPECT_EQ(bh.at(0), 0xe729de3fec21e30bea3d56adb01ed14bc107273c2775f9355afb10f594a10d9e_bytes32);
    EXPECT_EQ(bh.at(1), 0xb5eee60b45801179cbde3781b9a5dee9b3111554618c9cda3d6f7e351fd41e0b_bytes32);
}
