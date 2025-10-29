// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, create2_factory)
{
    const auto factory_code =
        calldatacopy(0, 0, calldatasize()) + create2().input(0, calldatasize());
    const auto initcode = mstore8(0, push(0xFE)) + ret(0, 1);

    tx.to = To;
    tx.data = initcode;
    pre[*tx.to] = {.nonce = 1, .code = factory_code};

    const auto create_address = compute_create2_address(*tx.to, {}, initcode);
    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;  // CREATE caller's nonce must be bumped
    expect.post[create_address].code = bytes{0xFE};
}

TEST_F(state_transition, create_tx_empty)
{
    // The default transaction without "to" address is a create transaction.

    expect.post[compute_create_address(Sender, pre[Sender].nonce)] = {.nonce = 1, .code = bytes{}};

    // Example of checking the expected the post state MPT root hash.
    expect.state_hash = 0x8ae438f7a4a14dbc25410dfaa12e95e7b36f311ab904b4358c3b544e06df4c50_bytes32;
}

TEST_F(state_transition, create_tx)
{
    tx.data = mstore8(0, push(0xFE)) + ret(0, 1);

    const auto create_address = compute_create_address(Sender, pre[Sender].nonce);
    expect.post[create_address].code = bytes{0xFE};
}

TEST_F(state_transition, create_tx_failure)
{
    static constexpr auto create_address = 0x3442a1dec1e72f337007125aa67221498cdd759d_address;

    tx.data = bytecode{} + OP_INVALID;

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[create_address].exists = false;
}

TEST_F(state_transition, create2_max_nonce)
{
    tx.to = To;
    pre[*tx.to] = {.nonce = ~uint64_t{0}, .code = create2()};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;  // Nonce is unchanged.
}

TEST_F(state_transition, code_deployment_out_of_gas_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // 63/64 gas rule enabled
    block.base_fee = 0;
    const auto initcode = ret(0, 5000);  // create contract with a lot of zeros, deploy cost 1M

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 1000000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    expect.post[To].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, code_deployment_out_of_gas_f)
{
    rev = EVMC_FRONTIER;
    block.base_fee = 0;
    const auto initcode = ret(0, 1000);  // create contract with a lot of zeros

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 100000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    const auto created = compute_create_address(To, pre[To].nonce);
    expect.post[created].code = bytes{};  // code deployment failure creates empty account
    expect.post[created].nonce = 0;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(created);  // address of created empty
}

TEST_F(state_transition, code_deployment_out_of_gas_storage_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // 63/64 gas rule enabled
    block.base_fee = 0;
    const auto initcode = sstore(0, 1)     // set storage
                          + ret(0, 5000);  // create contract with a lot of zeros

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 1000000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    expect.post[To].storage[0x00_bytes32] = 0x00_bytes32;
}

TEST_F(state_transition, code_deployment_out_of_gas_storage_f)
{
    rev = EVMC_FRONTIER;
    block.base_fee = 0;
    const auto initcode = sstore(0, 1)     // set storage
                          + ret(0, 1000);  // create contract with a lot of zeros

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 100000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    expect.post[To].exists = true;
    const auto created = compute_create_address(To, pre[To].nonce);
    expect.post[created].code = bytes{};  // code deployment failure creates empty account
    expect.post[created].nonce = 0;
    expect.post[created].storage[0x00_bytes32] = 0x01_bytes32;  // storage stays
    expect.post[To].storage[0x00_bytes32] = to_bytes32(created);
    expect.gas_used = 93134;
}

TEST_F(state_transition, code_deployment_out_of_gas_refund_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // 63/64 gas rule enabled
    block.base_fee = 0;
    const auto initcode = sstore(0, 1)     // set storage
                          + sstore(0, 0)   // gas refund
                          + ret(0, 5000);  // create contract with a lot of zeros

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 1000000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    expect.post[To].storage[0x00_bytes32] = 0x00_bytes32;
    expect.gas_used = 990207;
}

TEST_F(state_transition, code_deployment_out_of_gas_refund_f)
{
    rev = EVMC_FRONTIER;
    block.base_fee = 0;
    const auto initcode = sstore(0, 1)     // set storage
                          + sstore(0, 0)   // gas refund
                          + ret(0, 1000);  // create contract with a lot of zeros

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    tx.gas_limit = 100000;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       sstore(0, create().input(32 - initcode.size(), initcode.size()))};

    expect.post[To].exists = true;
    const auto created = compute_create_address(To, pre[To].nonce);
    expect.post[created].code = bytes{};  // code deployment failure creates empty account
    expect.post[created].nonce = 0;
    expect.post[created].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(created);
    expect.gas_used = 83140;
}

TEST_F(state_transition, create_tx_collision)
{
    static constexpr auto CREATED = 0x3442a1dec1e72f337007125aa67221498cdd759d_address;

    pre[CREATED] = {.nonce = 2};

    expect.status = EVMC_FAILURE;
    expect.post[CREATED].nonce = 2;
}

TEST_F(state_transition, create_tx_collision_storage)
{
    rev = EVMC_LONDON;

    // Test create address collision with an account only having storage (EIP-7610).
    const auto created = compute_create_address(Sender, pre[Sender].nonce);
    pre[created] = {.storage = {{0x00_bytes32, 0x01_bytes32}}};

    expect.status = EVMC_FAILURE;
    expect.post[created].storage[0x00_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create_collision)
{
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.to = To;
    pre[*tx.to] = {.code = create()};
    pre[CREATED] = {.nonce = 2};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce + 1;
    expect.post[CREATED].nonce = pre[CREATED].nonce;
}

TEST_F(state_transition, create_collision_storage)
{
    rev = EVMC_LONDON;

    // Test create address collision with an account only having storage (EIP-7610).
    tx.to = To;
    pre[To] = {.code = sstore(0, create())};
    const auto created = compute_create_address(To, pre[To].nonce);
    pre[created] = {.storage = {{0x00_bytes32, 0x01_bytes32}}};

    expect.post[To].nonce = pre[To].nonce + 1;
    expect.post[To].storage[0x00_bytes32] = 0x00_bytes32;
    expect.post[created].storage[0x00_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, create_collision_revert)
{
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};
    pre[CREATED] = {.nonce = 2};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].nonce = pre[CREATED].nonce;
}

TEST_F(state_transition, create_prefunded_revert)
{
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};
    pre[CREATED] = {.balance = 2};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].nonce = pre[CREATED].nonce;
}

TEST_F(state_transition, create_revert)
{
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = false;
}

TEST_F(state_transition, create_revert_sd)
{
    rev = EVMC_SPURIOUS_DRAGON;
    block.base_fee = 0;
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = false;
}

TEST_F(state_transition, create_revert_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;
    block.base_fee = 0;
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = false;
}

TEST_F(state_transition, create_collision_empty_revert)
{
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};
    pre[CREATED] = {};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = true;
}

TEST_F(state_transition, create_collision_empty_revert_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;
    block.base_fee = 0;
    static constexpr auto CREATED = 0x8bbc3514477d75ec797bbe4e19d7961660bb849c_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = create() + OP_INVALID};
    pre[CREATED] = {};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = true;
}

TEST_F(state_transition, touch_create_collision_empty_revert)
{
    static constexpr auto CREATED = 0x11f72042f0f1c9d8a1aeffc3680d0b41dd7769a7_address;
    static constexpr auto REVERT_PROXY = 0x94_address;

    tx.to = To;
    pre[*tx.to] = {.code = call(CREATED) + call(REVERT_PROXY).gas(0xffff)};
    pre[REVERT_PROXY] = {.code = create() + OP_INVALID};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = false;
    expect.post[REVERT_PROXY].exists = true;
}

TEST_F(state_transition, touch_create_collision_empty_revert_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;
    block.base_fee = 0;
    static constexpr auto CREATED = 0x11f72042f0f1c9d8a1aeffc3680d0b41dd7769a7_address;
    static constexpr auto REVERT_PROXY = 0x94_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(CREATED) + call(REVERT_PROXY).gas(0xffff)};
    pre[REVERT_PROXY] = {.code = create() + OP_INVALID};

    expect.post[*tx.to].nonce = pre[*tx.to].nonce;
    expect.post[CREATED].exists = true;
    expect.post[REVERT_PROXY].exists = true;
}

TEST_F(state_transition, created_code_hash)
{
    const auto runtime_code = bytes{0xc0};
    ASSERT_EQ(runtime_code.size(), 1);
    const auto initcode = mstore8(0, push(runtime_code)) + ret(0, runtime_code.size());
    tx.to = To;
    pre[To] = {.code = mstore(0, push(initcode)) +
                       create().input(32 - initcode.size(), initcode.size()) +
                       sstore(0, bytecode{OP_EXTCODEHASH})};

    const auto created = compute_create_address(To, pre[To].nonce);
    expect.post[created].code = runtime_code;
    expect.post[To].storage[0x00_bytes32] = keccak256(runtime_code);
}
