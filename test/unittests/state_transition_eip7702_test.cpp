// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, eip7702_set_code_transaction)
{
    rev = EVMC_PRAGUE;

    constexpr auto authority = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {{.addr = delegate, .nonce = 0, .signer = authority}};
    pre[To] = {.code = ret(0)};

    expect.post[To].exists = true;
    expect.post[delegate].exists = true;
    expect.post[authority].nonce = 1;
    expect.post[authority].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
}

TEST_F(state_transition, eip7702_set_code_transaction_authority_is_sender)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    // Sender nonce is 1 in prestate, it is bumped once for tx and then another time for delegation
    tx.authorization_list = {{.addr = delegate, .nonce = 2, .signer = Sender}};
    pre[To] = {.code = ret(0)};

    expect.post[Sender].nonce = 3;
    expect.post[Sender].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
    expect.post[To].exists = true;
    expect.post[delegate].exists = true;
}

TEST_F(state_transition, eip7702_set_code_transaction_authority_is_to)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {{.addr = delegate, .nonce = 0, .signer = To}};

    expect.post[delegate].exists = true;
    expect.post[To].nonce = pre[To].nonce + 1;
    expect.post[To].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
}

TEST_F(state_transition, eip7702_extcodesize)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    pre[delegate] = {.code = 1024 * OP_JUMPDEST};
    tx.to = To;
    pre[To] = {.code = sstore(1, push(callee) + OP_EXTCODESIZE)};

    expect.post[callee].exists = true;
    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x17_bytes32;
}

TEST_F(state_transition, eip7702_extcodehash_delegation_to_empty)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    tx.to = To;
    pre[To] = {.code = sstore(0, push(callee) + OP_EXTCODEHASH) + sstore(1, 1)};

    expect.post[callee].exists = true;
    expect.post[delegate].exists = false;
    expect.post[To].storage[0x00_bytes32] = keccak256(bytes{0xef, 0x01, 0x00} + hex(delegate));
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eip7702_extcodecopy)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    tx.to = To;
    pre[To] = {.code = push(10) + push0() + push0() + push(callee) + OP_EXTCODECOPY +
                       sstore(0, mload(0)) + sstore(1, 1)};

    expect.post[callee].exists = true;
    expect.post[delegate].exists = false;
    expect.post[To].storage[0x00_bytes32] =
        0xef01000000000000000000000000000000000000000000000000000000000000_bytes32;
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, eip7702_call)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    pre[delegate] = {.code = sstore(0, 0x11)};
    tx.to = To;
    pre[To] = {.code = sstore(1, call(callee).gas(50'000))};

    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].storage[0x00_bytes32] = 0x11_bytes32;
}

TEST_F(state_transition, eip7702_call_with_value)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    pre[delegate] = {.code = sstore(0, 0x11)};
    tx.to = To;
    pre[To] = {.balance = 10, .code = sstore(1, call(callee).gas(50'000).value(10))};

    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[To].balance = 0;
    expect.post[callee].storage[0x00_bytes32] = 0x11_bytes32;
    expect.post[callee].balance = 10;
    expect.post[delegate].balance = 0;
}

TEST_F(state_transition, eip7702_call_warms_up_delegate)
{
    rev = EVMC_PRAGUE;

    constexpr auto callee = 0xca11ee_address;
    constexpr auto delegate = 0xde1e_address;
    pre[callee] = {.nonce = 1, .code = bytes{0xef, 0x01, 0x00} + hex(delegate)};
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    pre[To] = {.code = sstore(1, call(callee).gas(50'000)) + OP_GAS + call(delegate).gas(50'000) +
                       OP_GAS + OP_SWAP1 + push(2) + OP_SSTORE + OP_SWAP1 + OP_SUB + push(3) +
                       OP_SSTORE};

    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[To].storage[0x02_bytes32] = 0x01_bytes32;
    // 100 gas for warm call + 7 * 3 for argument pushes + 2 for GAS = 123 = 0x7b
    expect.post[To].storage[0x03_bytes32] = 0x7b_bytes32;
    expect.post[callee].exists = true;
}

TEST_F(state_transition, eip7702_transaction_from_delegated_account)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[Sender].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
    pre[delegate] = {.code = 1024 * OP_JUMPDEST};

    tx.to = To;
    pre[To] = {.code = sstore(1, OP_CALLER)};

    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = to_bytes32(Sender);
}

TEST_F(state_transition, eip7702_transaction_to_delegated_account)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[To].code = bytes{0xef, 0x01, 0x00} + hex(delegate);

    pre[delegate] = {.code = sstore(1, 1)};
    tx.to = To;
    pre[To] = {.code = sstore(1, OP_CALLER)};

    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = to_bytes32(Sender);
}

TEST_F(state_transition, eip7702_transaction_to_delegation_to_precompile)
{
    rev = EVMC_PRAGUE;

    constexpr auto ecadd_precompile = 0x06_address;  // reverts on invalid input
    pre[To].code = bytes{0xef, 0x01, 0x00} + hex(ecadd_precompile);

    tx.to = To;
    tx.data = "01"_hex;

    expect.status = EVMC_SUCCESS;
    expect.post[To].exists = true;
}

TEST_F(state_transition, eip7702_transaction_to_delegation_to_empty)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[To].code = bytes{0xef, 0x01, 0x00} + hex(delegate);

    tx.to = To;

    expect.status = EVMC_SUCCESS;
    expect.post[To].exists = true;
    expect.post[delegate].exists = false;
}

TEST_F(state_transition, eip7702_delegated_mode_propagation_call)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    constexpr auto identity_precompile = 0x04_address;
    pre[delegate] = {
        .code = call(identity_precompile).input(0, 10).gas(OP_GAS) + sstore(1, returndatasize())};
    pre[To].code = bytes{0xef, 0x01, 0x00} + hex(delegate);

    tx.to = To;

    expect.post[delegate].exists = true;
    expect.post[To].storage[0x01_bytes32] = 0x0a_bytes32;
}

TEST_F(state_transition, eip7702_selfdestruct)
{
    rev = EVMC_PRAGUE;
    constexpr auto callee = 0xca11ee_address;
    constexpr bytes32 salt{0xff};

    const auto deploy_code = bytecode{selfdestruct(0x00_address)};
    const auto initcode =
        mstore(0, push(deploy_code)) + ret(32 - deploy_code.size(), deploy_code.size());
    const auto deployed_address = compute_create2_address(To, salt, initcode);

    pre[To].code = mstore(0, push(initcode)) +
                   sstore(0, create2().input(32 - initcode.size(), initcode.size()).salt(salt)) +
                   sstore(1, call(callee).gas(OP_GAS));
    pre[callee].code = bytes{0xef, 0x01, 0x00} + hex(deployed_address);

    tx.to = To;

    expect.post[deployed_address].code = deploy_code;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(deployed_address);
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].code = bytes{0xef, 0x01, 0x00} + hex(deployed_address);
}

TEST_F(state_transition, eip7702_set_code_transaction_with_selfdestruct)
{
    rev = EVMC_PRAGUE;
    constexpr auto callee = 0xca11ee_address;
    constexpr bytes32 salt{0xff};

    const auto deploy_code = bytecode{selfdestruct(0x00_address)};
    const auto initcode =
        mstore(0, push(deploy_code)) + ret(32 - deploy_code.size(), deploy_code.size());
    const auto deployed_address = compute_create2_address(To, salt, initcode);

    pre[To].code = mstore(0, push(initcode)) +
                   sstore(0, create2().input(32 - initcode.size(), initcode.size()).salt(salt)) +
                   sstore(1, call(callee).gas(OP_GAS));

    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {{.addr = deployed_address, .nonce = 0, .signer = callee}};

    expect.post[deployed_address].code = deploy_code;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(deployed_address);
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].code = bytes{0xef, 0x01, 0x00} + hex(deployed_address);
}
