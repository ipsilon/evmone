// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, tx_legacy)
{
    rev = EVMC_ISTANBUL;
    block.base_fee = 0;  // should be 0 before London
    tx.type = Transaction::Type::legacy;
    tx.to = To;

    expect.post.at(Sender).nonce = pre[Sender].nonce + 1;
}

TEST_F(state_transition, tx_non_existing_sender)
{
    rev = EVMC_BERLIN;
    block.base_fee = 0;  // should be 0 before London
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.max_gas_price = 0;
    tx.max_priority_gas_price = 0;
    tx.nonce = 0;
    pre.erase(Sender);

    expect.status = EVMC_SUCCESS;
    expect.post.at(Sender).nonce = 1;
    expect.post[Coinbase].exists = false;
}

TEST_F(state_transition, invalid_tx_non_existing_sender)
{
    rev = EVMC_BERLIN;
    block.base_fee = 0;  // should be 0 before London
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.max_gas_price = 1;
    tx.max_priority_gas_price = 1;
    tx.nonce = 0;
    pre.erase(Sender);

    expect.tx_error = INSUFFICIENT_FUNDS;
    expect.post[Sender].exists = false;
}

TEST_F(state_transition, tx_blob_gas_price)
{
    rev = EVMC_CANCUN;
    tx.type = Transaction::Type::blob;
    tx.to = To;
    tx.gas_limit = 25000;
    tx.max_gas_price = block.base_fee;  // minimal gas price to make it
    tx.max_priority_gas_price = 0;
    tx.nonce = 1;
    tx.blob_hashes.emplace_back(
        0x0100000000000000000000000000000000000000000000000000000000000000_bytes32);
    tx.max_blob_gas_price = 1;

    block.excess_blob_gas = 0;
    block.blob_base_fee = 1;
    block.blob_gas_used = 786432;

    pre[tx.sender].balance = GAS_PER_BLOB + tx.gas_limit * tx.max_gas_price;

    expect.post[Coinbase].exists = false;  // all gas is burned, Coinbase gets nothing
    expect.status = EVMC_SUCCESS;
}

TEST_F(state_transition, empty_coinbase_fee_0_sd)
{
    rev = EVMC_SPURIOUS_DRAGON;
    block_reward = 0;
    block.base_fee = 0;  // should be 0 before London
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.max_gas_price = 0;
    tx.max_priority_gas_price = 0;
    pre[Coinbase] = {};
    expect.post[To].exists = false;
    expect.post[Coinbase].exists = false;
}

TEST_F(state_transition, empty_coinbase_fee_0_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;
    block_reward = 0;
    block.base_fee = 0;  // should be 0 before London
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.max_gas_price = 0;
    tx.max_priority_gas_price = 0;
    pre[Coinbase] = {};
    expect.post[To].exists = true;
    expect.post[Coinbase].balance = 0;
}

TEST_F(state_transition, access_list_storage)
{
    tx.to = To;
    tx.access_list = {{To, {0x01_bytes32}}};

    pre[To] = {.storage = {{0x01_bytes32, 0x01_bytes32}}, .code = sstore(2, sload(1))};

    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[To].storage[0x02_bytes32] = 0x01_bytes32;
    expect.gas_used = 47506;  // Without access list: 45206
}

TEST_F(state_transition, tx_data_min_cost_exec_0)
{
    // In this test we bump the gas used to MIN_GAS by EIP-7623. Execution gas is 0.
    rev = EVMC_PRAGUE;
    tx.to = To;
    tx.data = "0001"_hex;
    static constexpr auto MIN_GAS = 40 + 10;

    expect.gas_used = 21000 + MIN_GAS;
}

TEST_F(state_transition, tx_data_min_cost_exec_50)
{
    // In this test the MIN_GAS by EIP-7623 is equal to the execution gas (50).
    rev = EVMC_PRAGUE;
    tx.to = To;
    tx.data = "0001"_hex;
    static constexpr auto DATA_GAS = 16 + 4;
    static constexpr auto MIN_GAS = 40 + 10;

    pre[To] = {.code = (MIN_GAS - DATA_GAS) * OP_JUMPDEST};
    expect.gas_used = 21000 + MIN_GAS;
    expect.post[To].exists = true;
}

TEST_F(state_transition, tx_data_min_cost_exec_51)
{
    // In this test the execution gas (51) is above the MIN_GAS by EIP-7623.
    rev = EVMC_PRAGUE;
    tx.to = To;
    tx.data = "0001"_hex;
    static constexpr auto DATA_GAS = 16 + 4;
    static constexpr auto MIN_GAS = 40 + 10;

    pre[To] = {.code = (MIN_GAS - DATA_GAS + 1) * OP_JUMPDEST};
    expect.gas_used = 21000 + MIN_GAS + 1;
    expect.post[To].exists = true;
}
