// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <test/blockchaintest/blockchaintest.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

TEST(json_loader, blockchain_test)
{
    // NOTE: fake `rlp` field! Never decoded by loader, only size is read and checked.
    std::istringstream input{R"({
        "000-fork=Shanghai-fill_stack": {
            "blocks": [
                {
                    "blockHeader": {
                        "parentHash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700",
                        "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                        "coinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
                        "stateRoot": "0x07b66de4268c3c26af1346a37fd0bb1584401981aafbedaa7837593030b5f968",
                        "transactionsTrie": "0x2bad57b8521a8d2a492526aecdb0e1244a14e1bc52809a046ac46a863ed9e54d",
                        "receiptTrie": "0xc227e1c29620a6496364056ce59ec4f51ed6e7bc56425e213d0195f84544c2c3",
                        "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "number": "0x05",
                        "gasLimit": "0x016345785d8a0000",
                        "gasUsed": "0xbc5f",
                        "timestamp": "0x03e8",
                        "extraData": "0x00",
                        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "nonce": "0x0000000000000000",
                        "baseFeePerGas": "0x07",
                        "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                        "hash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531"
                    },
                    "transactions": [
                        {
                            "type": "0x00",
                            "chainId": "0x01",
                            "nonce": "0x00",
                            "gasPrice": "0x0a",
                            "gasLimit": "0x0186a0",
                            "to": "0x0000000000000000000000000000000000000100",
                            "value": "0x00",
                            "data": "0x",
                            "v": "0x25",
                            "r": "0x86ddb9352affa90c20d71652b049404d8abcc6575e8e4a2c0bb9aa73fad9001c",
                            "s": "0x1bb0d685e5589862ae3d2b083be59c4f754c326800dbc82712e9f81eebf2f61d",
                            "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b"
                        }
                    ],
                    "uncleHeaders": [{
                        "baseFeePerGas" : "0x0d",
                        "bloom" : "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "coinbase" : "0xb94f5374fce5ed0000000097c15331677e6ebf0b",
                        "difficulty" : "0x020000",
                        "extraData" : "0x42",
                        "gasLimit" : "0x2fefd8",
                        "gasUsed" : "0x00",
                        "hash" : "0x83f67e4827f9ec54f3c95d6b0a29e8510838f3ac43d9fdce7f2f5eff57446aba",
                        "mixHash" : "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "nonce" : "0x0000000000000000",
                        "number" : "0x03",
                        "parentHash" : "0xe7728c48165014b3756026376b7d6bf1b185112e0f0b59cf71bffeeda134f002",
                        "receiptTrie" : "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                        "stateRoot" : "0x1d4410100fc542814bcbaee92d805a2b008844408c5d7049bdcc285309e80ca5",
                        "timestamp" : "0x54c99451",
                        "transactionsTrie" : "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                        "uncleHash" : "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347"
                    }],
                    "withdrawals": [{
                        "address" : "0xc000000000000000000000000000000000000001",
                        "amount" : "0x0186a0",
                        "index" : "0x00",
                        "validatorIndex" : "0x00"
                    }],
                    "rlp": "0x00ff00ff00ff"
                }
            ],
            "genesisBlockHeader": {
                "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                "coinbase": "0x0000000000000000000000000000000000000000",
                "stateRoot": "0x6b1356f9f8d3bf201a9aa2c44f836ab60c92a349385625da90dce80eb6ecad44",
                "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "difficulty": "0x00",
                "number": "0x00",
                "gasLimit": "0x016345785d8a0000",
                "gasUsed": "0x00",
                "timestamp": "0x00",
                "extraData": "0x00",
                "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "nonce": "0x0000000000000000",
                "baseFeePerGas": "0x07",
                "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "hash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700"
            },
            "lastblockhash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531",
            "network": "Shanghai",
            "pre": {
                "0x0000000000000000000000000000000000000100": {
                    "nonce": "0x00",
                    "balance": "0x00",
                    "code": "0x5f",
                    "storage": {}
                },
                "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
                    "nonce": "0x00",
                    "balance": "0x3635c9adc5dea00000",
                    "code": "0x",
                    "storage": {}
                }
            },
            "postState": {
                "0x0000000000000000000000000000000000000100": {
                    "nonce": "0x00",
                    "balance": "0x00",
                    "code": "0x5f",
                    "storage": {
                        "0x00": "0x01"
                    }
                },
                "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba": {
                    "nonce": "0x00",
                    "balance": "0x02351d",
                    "code": "0x",
                    "storage": {}
                }
            },
            "sealEngine": "NoProof"
        }
        })"};

    const auto btt = load_blockchain_tests(input);

    EXPECT_EQ(btt.size(), 1);
    EXPECT_EQ(btt[0].test_blocks.size(), 1);
    EXPECT_EQ(btt[0].rev.get_revision(0), evmc_revision::EVMC_SHANGHAI);
    EXPECT_EQ(btt[0].name, "000-fork=Shanghai-fill_stack");
    EXPECT_EQ(btt[0].genesis_block_header.timestamp, 0);
    EXPECT_EQ(btt[0].genesis_block_header.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].genesis_block_header.base_fee_per_gas, 0x07);

    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].type, state::Transaction::Type::legacy);
    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].sender,
        0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b_address);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.number, 5);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.timestamp, 0x03e8);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.ommers.size(), 1);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.ommers[0].beneficiary,
        0xb94f5374fce5ed0000000097c15331677e6ebf0b_address);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.ommers[0].delta, 2);
    const auto& withdrawals = btt[0].test_blocks[0].block_info.withdrawals;
    EXPECT_EQ(withdrawals.size(), 1);
    EXPECT_EQ(withdrawals[0].recipient, 0xc000000000000000000000000000000000000001_address);
    EXPECT_EQ(withdrawals[0].amount_in_gwei, 0x0186a0);
    EXPECT_EQ(withdrawals[0].index, 0);
    EXPECT_EQ(withdrawals[0].validator_index, 0);
}

TEST(json_loader, blockchain_test_post_state_hash)
{
    // NOTE: fake `rlp` field! Never decoded by loader, only size is read and checked.
    std::istringstream input{R"({
        "000-fork=Shanghai-fill_stack": {
            "blocks": [
                {
                    "blockHeader": {
                        "parentHash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700",
                        "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                        "coinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
                        "stateRoot": "0x07b66de4268c3c26af1346a37fd0bb1584401981aafbedaa7837593030b5f968",
                        "transactionsTrie": "0x2bad57b8521a8d2a492526aecdb0e1244a14e1bc52809a046ac46a863ed9e54d",
                        "receiptTrie": "0xc227e1c29620a6496364056ce59ec4f51ed6e7bc56425e213d0195f84544c2c3",
                        "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "difficulty": "0x00",
                        "number": "0x05",
                        "gasLimit": "0x016345785d8a0000",
                        "gasUsed": "0xbc5f",
                        "timestamp": "0x03e8",
                        "extraData": "0x00",
                        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "nonce": "0x0000000000000000",
                        "baseFeePerGas": "0x07",
                        "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                        "hash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531"
                    },
                    "transactions": [
                        {
                            "type": "0x00",
                            "chainId": "0x01",
                            "nonce": "0x00",
                            "gasPrice": "0x0a",
                            "gasLimit": "0x0186a0",
                            "to": "0x0000000000000000000000000000000000000100",
                            "value": "0x00",
                            "data": "0x",
                            "v": "0x25",
                            "r": "0x86ddb9352affa90c20d71652b049404d8abcc6575e8e4a2c0bb9aa73fad9001c",
                            "s": "0x1bb0d685e5589862ae3d2b083be59c4f754c326800dbc82712e9f81eebf2f61d",
                            "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b"
                        }
                    ],
                    "uncleHeaders": [],
                    "withdrawals": [],
                    "rlp": "0x00ff00ff00ff"
                }
            ],
            "genesisBlockHeader": {
                "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                "coinbase": "0x0000000000000000000000000000000000000000",
                "stateRoot": "0x6b1356f9f8d3bf201a9aa2c44f836ab60c92a349385625da90dce80eb6ecad44",
                "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "difficulty": "0x00",
                "number": "0x00",
                "gasLimit": "0x016345785d8a0000",
                "gasUsed": "0x00",
                "timestamp": "0x00",
                "extraData": "0x00",
                "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "nonce": "0x0000000000000000",
                "baseFeePerGas": "0x07",
                "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "hash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700"
            },
            "lastblockhash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531",
            "network": "ShanghaiToCancunAtTime15k",
            "pre": {
                "0x0000000000000000000000000000000000000100": {
                    "nonce": "0x00",
                    "balance": "0x00",
                    "code": "0x5f",
                    "storage": {}
                },
                "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
                    "nonce": "0x00",
                    "balance": "0x3635c9adc5dea00000",
                    "code": "0x",
                    "storage": {}
                }
            },
            "postStateHash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531",
            "sealEngine": "NoProof"
        }
        })"};

    const auto btt = load_blockchain_tests(input);

    EXPECT_EQ(btt.size(), 1);
    EXPECT_EQ(btt[0].test_blocks.size(), 1);
    EXPECT_EQ(btt[0].rev.get_revision(0), evmc_revision::EVMC_SHANGHAI);
    EXPECT_EQ(btt[0].rev.get_revision(15'000), evmc_revision::EVMC_CANCUN);
    EXPECT_EQ(btt[0].name, "000-fork=Shanghai-fill_stack");
    EXPECT_EQ(btt[0].genesis_block_header.timestamp, 0);
    EXPECT_EQ(btt[0].genesis_block_header.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].genesis_block_header.base_fee_per_gas, 0x07);

    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].type, state::Transaction::Type::legacy);
    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].sender,
        0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b_address);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.number, 5);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.timestamp, 0x03e8);
    EXPECT_EQ(std::get<hash256>(btt[0].expectation.post_state),
        0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531_bytes32);
}

TEST(json_loader, blockchain_test_pre_paris)
{
    // NOTE: fake `rlp` field! Never decoded by loader, only size is read and checked.
    std::istringstream input{R"({
        "000-fork=Shanghai-fill_stack": {
            "blocks": [
                {
                    "blockHeader": {
                        "parentHash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700",
                        "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                        "coinbase": "0x2adc25665018aa1fe0e6bc666dac8fc2697ff9ba",
                        "stateRoot": "0x07b66de4268c3c26af1346a37fd0bb1584401981aafbedaa7837593030b5f968",
                        "transactionsTrie": "0x2bad57b8521a8d2a492526aecdb0e1244a14e1bc52809a046ac46a863ed9e54d",
                        "receiptTrie": "0xc227e1c29620a6496364056ce59ec4f51ed6e7bc56425e213d0195f84544c2c3",
                        "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                        "difficulty": "0x20000",
                        "number": "0x05",
                        "gasLimit": "0x016345785d8a0000",
                        "gasUsed": "0xbc5f",
                        "timestamp": "0x03e8",
                        "extraData": "0x00",
                        "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                        "nonce": "0x0000000000000000",
                        "baseFeePerGas": "0x07",
                        "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                        "hash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531"
                    },
                    "transactions": [
                        {
                            "type": "0x00",
                            "chainId": "0x01",
                            "nonce": "0x00",
                            "gasPrice": "0x0a",
                            "gasLimit": "0x0186a0",
                            "to": "0x0000000000000000000000000000000000000100",
                            "value": "0x00",
                            "data": "0x",
                            "v": "0x25",
                            "r": "0x86ddb9352affa90c20d71652b049404d8abcc6575e8e4a2c0bb9aa73fad9001c",
                            "s": "0x1bb0d685e5589862ae3d2b083be59c4f754c326800dbc82712e9f81eebf2f61d",
                            "sender": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b"
                        }
                    ],
                    "uncleHeaders": [],
                    "withdrawals": [],
                    "rlp": "0x00ff00ff00ff"
                }
            ],
            "genesisBlockHeader": {
                "parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "uncleHash": "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347",
                "coinbase": "0x0000000000000000000000000000000000000000",
                "stateRoot": "0x6b1356f9f8d3bf201a9aa2c44f836ab60c92a349385625da90dce80eb6ecad44",
                "transactionsTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "receiptTrie": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "bloom": "0x00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "difficulty": "0x00",
                "number": "0x00",
                "gasLimit": "0x016345785d8a0000",
                "gasUsed": "0x00",
                "timestamp": "0x00",
                "extraData": "0x00",
                "mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
                "nonce": "0x0000000000000000",
                "baseFeePerGas": "0x07",
                "withdrawalsRoot": "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421",
                "hash": "0xe1bcc830589216abdc79cb3075f06f7b133f7b0cf257ecb346da33c354099700"
            },
            "lastblockhash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531",
            "network": "London",
            "pre": {
                "0x0000000000000000000000000000000000000100": {
                    "nonce": "0x00",
                    "balance": "0x00",
                    "code": "0x5f",
                    "storage": {}
                },
                "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b": {
                    "nonce": "0x00",
                    "balance": "0x3635c9adc5dea00000",
                    "code": "0x",
                    "storage": {}
                }
            },
            "postStateHash": "0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531",
            "sealEngine": "NoProof"
        }
        })"};

    const auto btt = load_blockchain_tests(input);

    EXPECT_EQ(btt.size(), 1);
    EXPECT_EQ(btt[0].test_blocks.size(), 1);
    EXPECT_EQ(btt[0].rev.get_revision(0), evmc_revision::EVMC_LONDON);
    EXPECT_EQ(btt[0].name, "000-fork=Shanghai-fill_stack");
    EXPECT_EQ(btt[0].genesis_block_header.timestamp, 0);
    EXPECT_EQ(btt[0].genesis_block_header.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].genesis_block_header.base_fee_per_gas, 0x07);

    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].type, state::Transaction::Type::legacy);
    EXPECT_EQ(btt[0].test_blocks[0].transactions[0].sender,
        0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b_address);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.gas_limit, 0x016345785d8a0000);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.number, 5);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.timestamp, 0x03e8);
    EXPECT_EQ(std::get<hash256>(btt[0].expectation.post_state),
        0x01de610f00331cea813e8143d51eb44ca352cdd90c602bb4b4bcf3c6cf9d5531_bytes32);
    EXPECT_EQ(btt[0].test_blocks[0].block_info.prev_randao,
        0x0000000000000000000000000000000000000000000000000000000000020000_bytes32);
}
