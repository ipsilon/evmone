// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, transient_storage)
{
    rev = EVMC_CANCUN;
    const auto tbump = 0xb0_address;

    tx.to = To;
    pre[tbump] = {.code = tstore(0, add(tload(0), 1)) + sstore(0, tload(0))};
    pre[*tx.to] = {.code = call(tbump).gas(0xffff) + call(tbump).gas(0xffff)};

    expect.post[To].exists = true;
    expect.post[tbump].storage[0x00_bytes32] = 0x02_bytes32;
}

TEST_F(state_transition, transient_storage_revert)
{
    rev = EVMC_CANCUN;
    const auto tbump = 0xb0_address;

    tx.to = To;
    pre[tbump] = {.code = tstore(0, add(tload(0), 1)) + sstore(0, tload(0))};
    pre[*tx.to] = {
        .code = call(tbump).gas(0xffff) + call(tbump).gas(0x1ff) + call(tbump).gas(0xffff)};

    expect.post[To].exists = true;
    expect.post[tbump].storage[0x00_bytes32] = 0x02_bytes32;
}

TEST_F(state_transition, transient_storage_static)
{
    rev = EVMC_CANCUN;
    const auto db = 0xdb_address;

    tx.to = To;
    pre[db] = {.code = tload(1) + jumpi(17, calldataload(0)) + ret_top() + OP_JUMPDEST +
                       tstore(1, add(7))};
    pre[*tx.to] = {.code = mstore(0, 1) +
                           // bump db.tstore[1] += 7
                           sstore(0xc1, call(db).gas(0xffff).input(0, 32)) +
                           // get db.tstore[1]
                           sstore(0xc2, staticcall(db).gas(0xffff).output(0, 32)) +
                           // sstore[0xd1] = db.tstore[1]
                           sstore(0xd1, mload(0)) +
                           // static call to bump db.tstore[1] fails
                           sstore(0xc3, staticcall(db).gas(0xffff).input(0, 32))};

    expect.post[db].exists = true;
    expect.post[To].exists = true;
    expect.post[To].storage[0xc1_bytes32] = 0x01_bytes32;
    expect.post[To].storage[0xc2_bytes32] = 0x01_bytes32;
    expect.post[To].storage[0xc3_bytes32] = 0x00_bytes32;
    expect.post[To].storage[0xd1_bytes32] = 0x07_bytes32;
}
