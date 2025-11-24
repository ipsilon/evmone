// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <intx/intx.hpp>
#include <test/utils/statetest.hpp>

using namespace evmone;
using namespace intx::literals;
using namespace testing;

// TODO: Add specific test of loading nonce, chainId, r, s, v

TEST(statetest_loader, tx_to_empty_string)
{
    // The "to":"" is correctly parsed as a creation transaction.
    constexpr std::string_view input = R"({
        "to": "",
        "input": "", "gas": "1", "chainId": "1", "value": "0", "sender": "a0a1",
        "gasPrice": "1", "nonce": "0", "v": "1",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_FALSE(tx.to.has_value());
}

TEST(statetest_loader, tx_to_null)
{
    // The "to":null is correctly parsed as a creation transaction.
    constexpr std::string_view input = R"({
        "to": null,
        "input": "", "gas": "1", "chainId": "1", "value": "0", "sender": "a0a1",
        "gasPrice": "1", "nonce": "0", "v": "1",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_FALSE(tx.to.has_value());
}

TEST(statetest_loader, tx_create_legacy)
{
    constexpr std::string_view input = R"({
        "input": "b0b1",
        "gas": "0x9091",
        "chainId": "0x5",
        "value": "0xe0e1",
        "sender": "a0a1",
        "gasPrice": "0x7071",
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.type, state::Transaction::Type::legacy);
    EXPECT_EQ(tx.data, (bytes{0xb0, 0xb1}));
    EXPECT_EQ(tx.gas_limit, 0x9091);
    EXPECT_EQ(tx.chain_id, 5);
    EXPECT_EQ(tx.value, 0xe0e1);
    EXPECT_EQ(tx.sender, 0xa0a1_address);
    EXPECT_FALSE(tx.to.has_value());
    EXPECT_EQ(tx.max_gas_price, 0x7071);
    EXPECT_EQ(tx.max_priority_gas_price, 0x7071);
    EXPECT_TRUE(tx.access_list.empty());
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.r, 0x1111111111111111111111111111111111111111111111111111111111111111_u256);
    EXPECT_EQ(tx.s, 0x2222222222222222222222222222222222222222222222222222222222222222_u256);
    EXPECT_EQ(tx.v, 1);
}

TEST(statetest_loader, tx_eip1559)
{
    constexpr std::string_view input = R"({
        "input": "b0b1",
        "gas": "0x9091",
        "value": "0xe0e1",
        "sender": "a0a1",
        "to": "c0c1",
        "maxFeePerGas": "0x7071",
        "maxPriorityFeePerGas": "0x6061",
        "accessList": [],
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.type, state::Transaction::Type::eip1559);
    EXPECT_EQ(tx.data, (bytes{0xb0, 0xb1}));
    EXPECT_EQ(tx.gas_limit, 0x9091);
    EXPECT_EQ(tx.chain_id, 1);
    EXPECT_EQ(tx.value, 0xe0e1);
    EXPECT_EQ(tx.sender, 0xa0a1_address);
    EXPECT_EQ(tx.to, 0xc0c1_address);
    EXPECT_EQ(tx.max_gas_price, 0x7071);
    EXPECT_EQ(tx.max_priority_gas_price, 0x6061);
    EXPECT_TRUE(tx.access_list.empty());
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.r, 0x1111111111111111111111111111111111111111111111111111111111111111_u256);
    EXPECT_EQ(tx.s, 0x2222222222222222222222222222222222222222222222222222222222222222_u256);
    EXPECT_EQ(tx.v, 1);
}

TEST(statetest_loader, tx_access_list)
{
    constexpr std::string_view input = R"({
        "input": "",
        "gas": "0",
        "value": "0",
        "sender": "",
        "maxFeePerGas": "0",
        "maxPriorityFeePerGas": "0",
        "accessList": [
            {"address": "ac01", "storageKeys": []},
            {"address": "ac02", "storageKeys": ["fe", "00"]}
        ],
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.type, state::Transaction::Type::eip1559);
    EXPECT_TRUE(tx.data.empty());
    EXPECT_EQ(tx.gas_limit, 0);
    EXPECT_EQ(tx.value, 0);
    EXPECT_EQ(tx.sender, address{});  // TODO: use 0x0_address?
    EXPECT_FALSE(tx.to.has_value());
    EXPECT_EQ(tx.max_gas_price, 0);
    EXPECT_EQ(tx.max_priority_gas_price, 0);
    ASSERT_EQ(tx.access_list.size(), 2);
    EXPECT_EQ(tx.access_list[0].first, 0xac01_address);
    EXPECT_EQ(tx.access_list[0].second.size(), 0);
    EXPECT_EQ(tx.access_list[1].first, 0xac02_address);
    EXPECT_EQ(tx.access_list[1].second, (std::vector{0xfe_bytes32, 0x00_bytes32}));
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.r, 0x1111111111111111111111111111111111111111111111111111111111111111_u256);
    EXPECT_EQ(tx.s, 0x2222222222222222222222222222222222222222222222222222222222222222_u256);
    EXPECT_EQ(tx.v, 1);
}

TEST(statetest_loader, tx_confusing)
{
    constexpr std::string_view input = R"({
        "input": "b0b1",
        "gas": "9091",
        "value": "0xe0e1",
        "sender": "a0a1",
        "to": "c0c1",
        "gasPrice": "0x8081",
        "maxFeePerGas": "0x7071",
        "maxPriorityFeePerGas": "0x6061",
        "accessList": [],
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    EXPECT_THAT([&] { test::from_json<state::Transaction>(json::json::parse(input)); },
        ThrowsMessage<std::invalid_argument>(
            "invalid transaction: contains both legacy and EIP-1559 fees"));
}

TEST(statetest_loader, tx_type_1)
{
    constexpr std::string_view input = R"({
        "input": "",
        "gas": "0",
        "type": "1",
        "value": "0",
        "sender": "",
        "gasPrice": "0",
        "accessList": [
            {"address": "ac01", "storageKeys": []},
            {"address": "ac02", "storageKeys": ["fe", "00"]}
        ],
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.type, state::Transaction::Type::access_list);
    EXPECT_TRUE(tx.data.empty());
    EXPECT_EQ(tx.gas_limit, 0);
    EXPECT_EQ(tx.value, 0);
    EXPECT_EQ(tx.sender, 0x00_address);
    EXPECT_FALSE(tx.to.has_value());
    EXPECT_EQ(tx.max_gas_price, 0);
    EXPECT_EQ(tx.max_priority_gas_price, 0);
    ASSERT_EQ(tx.access_list.size(), 2);
    EXPECT_EQ(tx.access_list[0].first, 0xac01_address);
    EXPECT_EQ(tx.access_list[0].second.size(), 0);
    EXPECT_EQ(tx.access_list[1].first, 0xac02_address);
    EXPECT_EQ(tx.access_list[1].second, (std::vector{0xfe_bytes32, 0x00_bytes32}));
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.r, 0x1111111111111111111111111111111111111111111111111111111111111111_u256);
    EXPECT_EQ(tx.s, 0x2222222222222222222222222222222222222222222222222222222222222222_u256);
    EXPECT_EQ(tx.v, 1);
}

TEST(statetest_loader, tx_type_3)
{
    constexpr std::string_view input = R"({
        "input": "",
        "gas": "0",
        "type": "3",
        "value": "0",
        "sender": "",
        "maxFeePerGas": "0",
        "maxPriorityFeePerGas": "0",
        "accessList": [
            {"address": "ac01", "storageKeys": []},
            {"address": "ac02", "storageKeys": ["fe", "00"]}
        ],
        "maxFeePerBlobGas": "1",
        "blobVersionedHashes": [
            "0x0111111111111111111111111111111111111111111111111111111111111111",
            "0x0222222222222222222222222222222222222222222222222222222222222222"
        ],
        "nonce": "0",
        "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
        "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
        "v": "1"
    })";

    const auto tx = test::from_json<state::Transaction>(json::json::parse(input));
    EXPECT_EQ(tx.type, state::Transaction::Type::blob);
    EXPECT_TRUE(tx.data.empty());
    EXPECT_EQ(tx.gas_limit, 0);
    EXPECT_EQ(tx.value, 0);
    EXPECT_EQ(tx.sender, 0x00_address);
    EXPECT_FALSE(tx.to.has_value());
    EXPECT_EQ(tx.max_gas_price, 0);
    EXPECT_EQ(tx.max_priority_gas_price, 0);
    ASSERT_EQ(tx.access_list.size(), 2);
    EXPECT_EQ(tx.access_list[0].first, 0xac01_address);
    EXPECT_EQ(tx.access_list[0].second.size(), 0);
    EXPECT_EQ(tx.access_list[1].first, 0xac02_address);
    EXPECT_EQ(tx.access_list[1].second, (std::vector{0xfe_bytes32, 0x00_bytes32}));
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.r, 0x1111111111111111111111111111111111111111111111111111111111111111_u256);
    EXPECT_EQ(tx.s, 0x2222222222222222222222222222222222222222222222222222222222222222_u256);
    EXPECT_EQ(tx.v, 1);
    EXPECT_EQ(tx.max_blob_gas_price, 1);
    EXPECT_EQ(tx.blob_hashes.size(), 2);
    EXPECT_EQ(tx.blob_hashes[0],
        0x0111111111111111111111111111111111111111111111111111111111111111_bytes32);
}

TEST(statetest_loader, tx_invalid_blob_versioned_hash)
{
    constexpr std::string_view input = R"({
        "input" : "0x00",
        "gas" : "0x3d0900",
        "nonce" : "0x0",
        "to" : "0x095e7baea6a6c7c4c2dfeb977efac326af552d87",
        "value" : "0x186a0",
        "v" : "0x0",
        "r" : "0xbf751ed5c37bd65d3ace5b73a1c62f7388b203a82ce366392e7b76fd2de12cb1",
        "s" : "0x6f2b5344e5b997d35b3a0768006196a65c4eff8ed3acad5201a105c2b59b4e8c",
        "secretKey" : "0x45a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8",
        "chainId" : "0x1",
        "type" : "0x3",
        "maxFeePerGas" : "0x12a05f200",
        "maxPriorityFeePerGas" : "0x2",
        "accessList" : [
            {
                "address" : "0x095e7baea6a6c7c4c2dfeb977efac326af552d87",
                "storageKeys" : [
                    "0x0000000000000000000000000000000000000000000000000000000000000000",
                    "0x0000000000000000000000000000000000000000000000000000000000000001"
                ]
            }
        ],
        "maxFeePerBlobGas" : "0xa",
        "blobVersionedHashes" : [
            "0x1a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8",
            "0x1a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8"
        ],
        "hash" : "0x6f26856255f46b27b31d06e00750d8d75067fd8a28e15e8f5557a33fba288cb5",
        "sender" : "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b"
    })";

    EXPECT_THAT([&] { test::from_json<state::Transaction>(json::json::parse(input)); },
        ThrowsMessage<std::invalid_argument>(
            "invalid hash: 0x1a915e4d060149eb4365960e6a7a45f334393093061116b197e3240065ff2d8"));
}

TEST(statetest_loader, invalid_tx_type)
{
    {
        constexpr std::string_view input = R"({
                "input": "",
                "gas": "0",
                "type": "2",
                "value": "0",
                "sender": "",
                "gasPrice": "0",
                "accessList": [
                    {"address": "ac01", "storageKeys": []},
                    {"address": "ac02", "storageKeys": ["fe", "00"]}
                ],
                "nonce": "0",
                "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
                "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
                "v": "1"
            })";

        EXPECT_THAT([&] { test::from_json<state::Transaction>(json::json::parse(input)); },
            ThrowsMessage<std::invalid_argument>("wrong transaction type: 2, expected: 1"));
    }
    {
        constexpr std::string_view input = R"({
            "input": "",
            "gas": "0",
            "type": "1",
            "value": "0",
            "sender": "",
            "gasPrice": "0",
            "nonce": "0",
            "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
            "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
            "v": "1"
        })";

        EXPECT_THAT([&] { test::from_json<state::Transaction>(json::json::parse(input)); },
            ThrowsMessage<std::invalid_argument>("wrong transaction type: 1, expected: 0"));
    }

    {
        constexpr std::string_view input = R"({
            "input": "",
            "gas": "0",
            "type": "1",
            "value": "0",
            "sender": "",
            "maxFeePerGas": "0",
            "maxPriorityFeePerGas": "0",
            "accessList": [
                {"address": "ac01", "storageKeys": []},
                {"address": "ac02", "storageKeys": ["fe", "00"]}
            ],
            "nonce": "0",
            "r": "0x1111111111111111111111111111111111111111111111111111111111111111",
            "s": "0x2222222222222222222222222222222222222222222222222222222222222222",
            "v": "1"
        })";

        EXPECT_THAT([&] { test::from_json<state::Transaction>(json::json::parse(input)); },
            ThrowsMessage<std::invalid_argument>("wrong transaction type: 1, expected: 2"));
    }
}

namespace evmone::test
{
// This function is used only by the following test case and in `statetest_loader.cpp` where it is
// defined.
template <>
uint8_t from_json<uint8_t>(const json::json& j);
}  // namespace evmone::test

TEST(statetest_loader, load_uint8_t)
{
    {
        constexpr std::string_view input = R"({
            "v": "0xFF"
        })";

        EXPECT_EQ(test::from_json<uint8_t>(json::json::parse(input)["v"]), 255);
    }
    {
        constexpr std::string_view input = R"({
            "v": "0x100"
        })";

        EXPECT_THROW(test::from_json<uint8_t>(json::json::parse(input)["v"]), std::out_of_range);
    }
}
