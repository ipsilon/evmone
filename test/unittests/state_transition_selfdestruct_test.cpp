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
    tx.data = selfdestruct(0xbe_address);
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
