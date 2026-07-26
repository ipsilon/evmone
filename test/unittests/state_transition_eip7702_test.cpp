// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"
#include <evmone/constants.hpp>

using namespace evmc::literals;
using namespace intx;
using namespace evmone::test;

namespace
{
// Authorization tuples signed by AUTHORITY and Sender with the keys the fixture documents.
// evmone has no ECDSA signer, so the signatures are literals: changing chain_id, addr or nonce
// means re-signing keccak256(0x05 || rlp([chain_id, addr, nonce])).

constexpr Authorization AUTHORITY_DELEGATION{
    .addr = 0xde1e_address,
    .nonce = 0,
    .y_parity = 0,
    .r = 0x7cb2b4929dfbe4d0fb2aa6f22a5aa484e1f1bf465908045f913a2e6aa0850826_u256,
    .s = 0x7741194e3500b9c5bf376a0ca7bfb813555289d035bc13c7aa188052328c0521_u256,
};

constexpr Authorization AUTHORITY_DELEGATION_NONCE1{
    .addr = 0xde1e_address,
    .nonce = 1,
    .y_parity = 1,
    .r = 0xa565c4e16dd98f02b76f754c1bad064cf1399948dd1a7ba6bcd861a4c92caf03_u256,
    .s = 0x2b6dd08f993e76340c0e9bb41e67fda24eefdbf815ebb38f99876a573a674834_u256,
};

constexpr Authorization SENDER_DELEGATION_NONCE2{
    .addr = 0xde1e_address,
    .nonce = 2,
    .y_parity = 1,
    .r = 0xa7a87fd5ac72c2a06ce5de51b42246cc39ee17fd1b9b7fa1600af4b9f200e401_u256,
    .s = 0x5e690658827f7efd3415920c55057a50a97db979e53fcc5764e264af2c40ce03_u256,
};

// Signed over MAX_NONCE - 1, the last nonce a sender can still authorize from.
constexpr Authorization SENDER_DELEGATION_NONCE_MAX{
    .addr = 0xde1e_address,
    .nonce = MAX_NONCE - 1,
    .y_parity = 1,
    .r = 0x091b7ea090552d9b423479ff48bab00ddc4c87e7d610c6fa0dfe7a57019dcde2_u256,
    .s = 0x7a5c56d5724cdc6b49f8d39b1dda26bf9a660745e2eeba6bd05c27d56d030fca_u256,
};

// The CREATE2 address eip7702_set_code_transaction_with_selfdestruct deploys; that test asserts
// the deployment still lands here, because the authorization below is signed over it.
constexpr auto SELFDESTRUCTING_DELEGATE = 0x917e75ff40e354f8d10ed4456ae7e365e7ee7912_address;

constexpr Authorization AUTHORITY_DELEGATION_TO_SELFDESTRUCTING{
    .addr = SELFDESTRUCTING_DELEGATE,
    .nonce = 0,
    .y_parity = 0,
    .r = 0xe26a6ca88e50d3559040a6962e36eccf4577388da303f5f85664c0c103b7d3fa_u256,
    .s = 0x7ed6094cbae06609e21276851cf382befb3ee9d91ae402ef5dc9169343cab8c3_u256,
};
}  // namespace

TEST_F(state_transition, eip7702_set_code_transaction)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {AUTHORITY_DELEGATION};
    pre[To] = {.code = ret(0)};

    expect.post[To].exists = true;
    expect.post[delegate].exists = true;
    expect.post[AUTHORITY].nonce = 1;
    expect.post[AUTHORITY].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
}

TEST_F(state_transition, eip7702_set_code_transaction_authority_is_sender)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    // Sender nonce is 1 in prestate, it is bumped once for tx and then another time for delegation
    tx.authorization_list = {SENDER_DELEGATION_NONCE2};
    pre[To] = {.code = ret(0)};

    expect.post[Sender].nonce = 3;
    expect.post[Sender].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
    expect.post[To].exists = true;
    expect.post[delegate].exists = true;
}

TEST_F(state_transition, eip7702_set_code_self_authorization_reaching_nonce_max)
{
    // A self-authorization that bumps the sender nonce to MAX_NONCE (2^64-1) is valid: only a
    // tx nonce == MAX_NONCE is rejected by EIP-2681, not reaching it during execution.
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;

    // The tx bumps the sender nonce 2^64-3 -> 2^64-2, then the self-authorization -> 2^64-1.
    pre[Sender].nonce = MAX_NONCE - 2;
    tx.nonce = MAX_NONCE - 2;
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {SENDER_DELEGATION_NONCE_MAX};
    pre[To] = {.code = sstore(0, 1)};

    expect.status = EVMC_SUCCESS;
    expect.post[Sender].nonce = MAX_NONCE;
    expect.post[Sender].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
    expect.post[To].storage[0x00_bytes32] = 0x01_bytes32;  // Proves the top-level call executed.
}

TEST_F(state_transition, eip7702_set_code_transaction_authority_is_to)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[delegate] = {.code = bytecode{OP_STOP}};
    tx.to = AUTHORITY;  // The authority is also the transaction destination.
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {AUTHORITY_DELEGATION};

    expect.post[delegate].exists = true;
    expect.post[AUTHORITY].nonce = pre[AUTHORITY].nonce + 1;
    expect.post[AUTHORITY].code = bytes{0xef, 0x01, 0x00} + hex(delegate);
}

TEST_F(state_transition, eip7702_set_code_transaction_invalid_y_parity)
{
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    pre[AUTHORITY] = {.nonce = 1};
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    auto auth = AUTHORITY_DELEGATION_NONCE1;
    auth.y_parity = 2;  // Corrupt the y_parity of an otherwise valid signature.
    tx.authorization_list = {auth};
    pre[To] = {.code = ret(0)};

    expect.post[AUTHORITY].nonce = 1;
    expect.post[AUTHORITY].code = bytes{};
    expect.post[To].exists = true;
    expect.post[delegate].exists = false;
}

TEST_F(state_transition, eip7702_set_code_transaction_unrecoverable_signature)
{
    // An authorization with no recoverable authority must be skipped, leaving no account behind:
    // https://github.com/ipsilon/evmone/issues/1483.
    rev = EVMC_PRAGUE;

    constexpr auto delegate = 0xde1e_address;
    tx.to = To;
    tx.type = Transaction::Type::set_code;
    auto auth = AUTHORITY_DELEGATION;
    auth.r = 0;
    auth.s = 0;
    tx.authorization_list = {auth};
    pre[To] = {.code = ret(0)};

    expect.post[To].exists = true;
    expect.post[AUTHORITY].exists = false;
    expect.post[delegate].exists = false;
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
    const auto callee = AUTHORITY;  // The delegation is installed on the called account.
    constexpr bytes32 salt{0xff};

    const auto deploy_code = bytecode{selfdestruct(0x00_address)};
    const auto initcode =
        mstore(0, push(deploy_code)) + ret(32 - deploy_code.size(), deploy_code.size());
    const auto deployed_address = compute_create2_address(To, salt, initcode);
    ASSERT_EQ(deployed_address, SELFDESTRUCTING_DELEGATE);

    pre[To].code = mstore(0, push(initcode)) +
                   sstore(0, create2().input(32 - initcode.size(), initcode.size()).salt(salt)) +
                   sstore(1, call(callee).gas(OP_GAS));

    tx.to = To;
    tx.type = Transaction::Type::set_code;
    tx.authorization_list = {AUTHORITY_DELEGATION_TO_SELFDESTRUCTING};

    expect.post[deployed_address].code = deploy_code;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(deployed_address);
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[callee].code = bytes{0xef, 0x01, 0x00} + hex(deployed_address);
}
