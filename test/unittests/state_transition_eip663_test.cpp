// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, dupn)
{
    rev = EVMC_EXPERIMENTAL;
    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(
            push(1) + 255 * push(2) + OP_DUPN + "ff" + sstore(0) + sstore(1) + OP_STOP, 258),
    };
    expect.post[*tx.to].storage[0x00_bytes32] = 0x01_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
}

TEST_F(state_transition, swapn)
{
    rev = EVMC_EXPERIMENTAL;
    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(
            push(1) + 256 * push(2) + OP_SWAPN + "ff" + sstore(0) + sstore(1) + OP_STOP, 258),
    };
    expect.post[*tx.to].storage[0x00_bytes32] = 0x01_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x02_bytes32;
}

TEST_F(state_transition, exchange)
{
    rev = EVMC_EXPERIMENTAL;
    tx.to = To;
    pre[*tx.to] = {
        .code = eof_bytecode(push(1) + push(2) + push(3) + OP_EXCHANGE + "00" + sstore(0) +
                                 sstore(1) + sstore(2) + OP_STOP,
            4),
    };
    expect.post[*tx.to].storage[0x00_bytes32] = 0x03_bytes32;
    expect.post[*tx.to].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[*tx.to].storage[0x02_bytes32] = 0x02_bytes32;
}
