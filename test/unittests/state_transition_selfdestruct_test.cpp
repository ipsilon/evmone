// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, selfdestruct_shanghai)
{
    rev = EVMC_SHANGHAI;
    tx.to = To;
    pre[*tx.to] = {.balance = 0x4e, .code = selfdestruct(0xbe_address)};

    expect.post[To].exists = false;
    expect.post[0xbe_address].balance = 0x4e;
}

TEST_F(state_transition, selfdestruct_cancun)
{
    rev = EVMC_CANCUN;
    tx.to = To;
    pre[*tx.to] = {.balance = 0x4e, .code = selfdestruct(0xbe_address)};

    expect.post[To].balance = 0;
    expect.post[0xbe_address].balance = 0x4e;
}

TEST_F(state_transition, selfdestruct_to_self_cancun)
{
    rev = EVMC_CANCUN;
    tx.to = To;
    pre[*tx.to] = {.balance = 0x4e, .code = selfdestruct(To)};

    expect.post[To].balance = 0x4e;
}

TEST_F(state_transition, selfdestruct_same_tx_cancun)
{
    rev = EVMC_CANCUN;
    tx.value = 0x4e;
    tx.data = selfdestruct(0xbe_address);
    pre[Sender].balance += 0x4e;

    expect.post[0xbe_address].balance = 0x4e;
}

TEST_F(state_transition, selfdestruct_same_create_cancun)
{
    // Use CREATE to temporarily create an account using initcode with SELFDESTRUCT.
    // The CREATE should succeed by returning proper address, but the created account
    // should not be in the post state.
    rev = EVMC_CANCUN;
    static constexpr auto BENEFICIARY = 0x4a0000be_address;
    const auto initcode = selfdestruct(BENEFICIARY);

    tx.to = To;
    pre[To] = {
        .balance = 0x4e,
        .code = mstore(0, push(initcode)) +
                create().input(32 - initcode.size(), initcode.size()).value(0x0e) + sstore(0),
    };

    expect.post[To].balance = 0x40;
    expect.post[To].storage[0x00_bytes32] = to_bytes32(compute_create_address(To, pre[To].nonce));
    expect.post[BENEFICIARY].balance = 0x0e;
}

TEST_F(state_transition, selfdestruct_beneficiary_with_code)
{
    // Send ETH via SELFDESTRUCT to an account with code.
    // This test checks if the beneficiary's code in the state is not somehow disturbed
    // by this action as we likely don't load the code from database.
    rev = EVMC_CANCUN;
    static constexpr auto BENEFICIARY = 0x4a0000be_address;

    tx.to = To;
    pre[To] = {.balance = 1, .code = selfdestruct(BENEFICIARY)};
    pre[BENEFICIARY] = {.code = bytecode{OP_STOP}};

    expect.post[To].balance = 0;
    expect.post[BENEFICIARY].code = pre[BENEFICIARY].code;
}

TEST_F(state_transition, selfdestruct_double_revert)
{
    rev = EVMC_SHANGHAI;

    static constexpr auto CALL_PROXY = 0xc0_address;
    static constexpr auto REVERT_PROXY = 0xd0_address;
    static constexpr auto SELFDESTRUCT = 0xff_address;
    static constexpr auto BENEFICIARY = 0xbe_address;

    pre[SELFDESTRUCT] = {.balance = 1, .code = selfdestruct(BENEFICIARY)};
    pre[CALL_PROXY] = {.code = call(SELFDESTRUCT).gas(0xffffff)};
    pre[REVERT_PROXY] = {.code = call(SELFDESTRUCT).gas(0xffffff) + revert(0, 0)};
    pre[To] = {.code = call(CALL_PROXY).gas(0xffffff) + call(REVERT_PROXY).gas(0xffffff)};
    tx.to = To;

    expect.post[SELFDESTRUCT].exists = false;
    expect.post[CALL_PROXY].exists = true;
    expect.post[REVERT_PROXY].exists = true;
    expect.post[To].exists = true;
    expect.post[BENEFICIARY].balance = 1;
}

TEST_F(state_transition, selfdestruct_initcode)
{
    rev = EVMC_SHANGHAI;
    tx.data = selfdestruct(0xbe_address);

    expect.post[compute_create_address(tx.sender, tx.nonce)].exists = false;
    expect.post[0xbe_address].exists = false;
}

TEST_F(state_transition, selfdestruct_initcode_amsterdam)
{
    // A same-tx-created account that self-destructs ending with a zero balance must not be in the
    // final state (EIP-8246). In this test we use initcode.
    rev = EVMC_AMSTERDAM;
    tx.data = selfdestruct(0xbe_address);

    expect.post[compute_create_address(tx.sender, tx.nonce)].exists = false;
    expect.post[0xbe_address].exists = false;
}

TEST_F(state_transition, selfdestruct_prefunded)
{
    // Although burn is removed in EIP-8246, the deletion of a pre-funded account still happens.
    rev = EVMC_CANCUN;
    const auto created = compute_create_address(tx.sender, tx.nonce);
    pre[created] = {.balance = 1};
    tx.data = selfdestruct(0xbe_address);  // Transfer to distinct beneficiary.

    expect.post[created].exists = false;    // Removed, despite pre-existing in the state.
    expect.post[0xbe_address].balance = 1;  // Pre-funded balance delivered to the beneficiary.
}

TEST_F(state_transition, selfdestruct_prefunded_amsterdam)
{
    // Although burn is removed in EIP-8246, the deletion of a pre-funded account still happens.
    rev = EVMC_AMSTERDAM;
    const auto created = compute_create_address(tx.sender, tx.nonce);
    pre[created] = {.balance = 1};
    tx.data = selfdestruct(0xbe_address);  // Transfer to distinct beneficiary.

    expect.post[created].exists = false;    // Removed, despite pre-existing in the state.
    expect.post[0xbe_address].balance = 1;  // Pre-funded balance delivered to the beneficiary.
}

TEST_F(state_transition, selfdestruct_prefunded_burn)
{
    // Burn pre-funded ETH by self-destruct to self.
    rev = EVMC_CANCUN;
    const auto created = compute_create_address(tx.sender, tx.nonce);
    pre[created] = {.balance = 1};
    tx.data = selfdestruct(created);

    expect.post[created].exists = false;  // Removed, despite pre-existing in the state.
}

TEST_F(state_transition, selfdestruct_prefunded_burn_amsterdam)
{
    // Burn is removed with EIP-8246, the balance must be preserved.
    rev = EVMC_AMSTERDAM;
    const auto created = compute_create_address(tx.sender, tx.nonce);
    pre[created] = {.balance = 1};
    tx.data = selfdestruct(created);

    expect.post[created].balance = 1;  // Balance preserved.
    expect.post[created].nonce = 0;
    expect.post[created].code = {};
}

TEST_F(state_transition, selfdestruct_sibling_create_then_destruct_amsterdam)
{
    // A contract created in one sub-call and self-destructed in a sibling sub-call of the same
    // transaction must still be removed (especially its code).
    rev = EVMC_AMSTERDAM;
    static constexpr auto F = 0xfac0_address;  // Factory address.

    const auto runtime = selfdestruct(0xbe_address);
    const auto initcode = mstore(0, push(runtime)) + ret(32 - runtime.size(), runtime.size());
    const auto created = compute_create_address(F, 1);

    pre[F] = {.nonce = 1,
        .balance = 1,
        .code = mstore(0, push(initcode)) +
                create().input(32 - initcode.size(), initcode.size()).value(1)};
    pre[To] = {.code = call(F).gas(0xffffff) + call(created).gas(0xffffff)};
    tx.to = To;

    expect.post[created].exists = false;    // Created and destructed in the same tx -> removed.
    expect.post[0xbe_address].balance = 1;  // Funds delivered to the beneficiary.
    expect.post[F] = {.nonce = 2, .balance = 0};  // F created one contract and sent it the balance.
    expect.post[To] = {};
}

TEST_F(state_transition, massdestruct_shanghai)
{
    rev = EVMC_SHANGHAI;

    static constexpr auto BASE = 0xdead0000_address;
    static constexpr auto SINK = 0xbeef_address;
    static constexpr size_t N = 3930;

    const auto b = intx::be::load<intx::uint256>(BASE);
    const auto selfdestruct_code = selfdestruct(SINK);
    bytecode driver_code;
    for (size_t i = 0; i < N; ++i)
    {
        const auto a = intx::be::trunc<address>(b + i);
        pre[a] = {.balance = 1, .code = selfdestruct_code};
        driver_code += 5 * OP_PUSH0 + push(a) + OP_DUP1 + OP_CALL + OP_POP;
    }

    tx.to = To;
    tx.gas_limit = 30'000'000;
    block.gas_limit = tx.gas_limit;

    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price;
    pre[*tx.to] = {.code = driver_code};
    expect.post[*tx.to].exists = true;

    expect.post[SINK].balance = N;
}

TEST_F(state_transition, massdestruct_cancun)
{
    rev = EVMC_CANCUN;

    static constexpr auto BASE = 0xdead0000_address;
    static constexpr auto SINK = 0xbeef_address;
    static constexpr size_t N = 3930;

    const auto b = intx::be::load<intx::uint256>(BASE);
    const auto selfdestruct_code = selfdestruct(SINK);
    bytecode driver_code;
    for (size_t i = 0; i < N; ++i)
    {
        const auto a = intx::be::trunc<address>(b + i);
        pre[a] = {.balance = 1, .code = selfdestruct_code};
        driver_code += 5 * OP_PUSH0 + push(a) + OP_DUP1 + OP_CALL + OP_POP;
        expect.post[a].balance = 0;
    }

    tx.to = To;
    tx.gas_limit = 30'000'000;
    block.gas_limit = tx.gas_limit;

    pre[tx.sender].balance = tx.gas_limit * tx.max_gas_price;
    pre[*tx.to] = {.code = driver_code};
    expect.post[*tx.to].exists = true;

    expect.post[SINK].balance = N;
}

TEST_F(state_transition, eip7708_transfer_log_selfdestruct_existing)
{
    // A pre-existing contract self-destructing to a distinct beneficiary emits an ETH transfer log
    // for the moved balance (EIP-7708).
    rev = EVMC_AMSTERDAM;
    static constexpr auto Beneficiary = 0xbe_address;
    tx.to = To;
    pre[To] = {.balance = 0x99, .code = selfdestruct(Beneficiary)};

    expect.post[To] = {};  // EIP-6780: survives, balance moved out.
    expect.post[Beneficiary].balance = 0x99;
    expect.logs = {transfer_log(To, Beneficiary, 0x99)};
}

TEST_F(state_transition, eip7708_transfer_log_create_tx_then_selfdestruct)
{
    // A CREATE-transaction endowment emits an ETH transfer log, then the same-tx-created account
    // self-destructing in its initcode emits a second ETH transfer log (EIP-7708).
    rev = EVMC_AMSTERDAM;
    static constexpr auto Beneficiary = 0xbe_address;
    tx.value = 0x99;
    tx.data = selfdestruct(Beneficiary);
    pre[Sender].balance += 0x99;
    const auto created = compute_create_address(Sender, tx.nonce);

    expect.post[created].exists = false;
    expect.post[Beneficiary].balance = 0x99;
    expect.logs = {transfer_log(Sender, created, 0x99), transfer_log(created, Beneficiary, 0x99)};
}
