// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, call_value_to_empty)
{
    rev = EVMC_LONDON;
    static constexpr auto BENEFICIARY = 0xbe_address;
    tx.to = To;
    pre[To] = {.balance = 1, .code = call(BENEFICIARY).value(1)};
    pre[BENEFICIARY] = {};

    expect.post[To].balance = 0;
    expect.post[BENEFICIARY].balance = 1;
}

TEST_F(state_transition, delegatecall_static_legacy)
{
    rev = EVMC_LONDON;
    // Checks if DELEGATECALL forwards the "static" flag.
    static constexpr auto CALLEE1 = 0xca11ee01_address;
    static constexpr auto CALLEE2 = 0xca11ee02_address;
    pre[CALLEE2] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = sstore(1, 0xcc_bytes32),
    };
    pre[CALLEE1] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}},
        .code = ret(delegatecall(CALLEE2).gas(100'000)),
    };
    tx.to = To;
    pre[To] = {
        .storage = {{0x01_bytes32, 0xdd_bytes32}, {0x02_bytes32, 0xdd_bytes32}},
        .code = sstore(1, staticcall(CALLEE1).gas(200'000)) +
                sstore(2, returndatacopy(0, 0, returndatasize()) + mload(0)),
    };

    expect.gas_used = 131480;
    // Outer call - success.
    expect.post[To].storage[0x01_bytes32] = 0x01_bytes32;
    // Inner call - no success.
    expect.post[To].storage[0x02_bytes32] = 0x00_bytes32;
    // SSTORE failed.
    expect.post[CALLEE1].storage[0x01_bytes32] = 0xdd_bytes32;
    expect.post[CALLEE2].storage[0x01_bytes32] = 0xdd_bytes32;
}
