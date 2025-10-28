// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, extcodehash_existent)
{
    rev = EVMC_ISTANBUL;  // before account access
    block.base_fee = 0;

    static constexpr auto EXT = 0xe4_address;
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[To] = {.code = sstore(0, push(EXT) + OP_EXTCODEHASH)};
    pre[EXT] = {.code = bytecode{"1234"}};

    expect.post[EXT].exists = true;
    expect.post[To].storage[0x00_bytes32] = keccak256(pre[EXT].code);
}

TEST_F(state_transition, extcodesize_existent)
{
    rev = EVMC_ISTANBUL;  // before account access
    block.base_fee = 0;

    static constexpr auto EXT = 0xe4_address;
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[To] = {.code = sstore(0, push(EXT) + OP_EXTCODESIZE)};
    pre[EXT] = {.code = bytes(3, 0)};

    expect.post[EXT].exists = true;
    expect.post[To].storage[0x00_bytes32] = 0x03_bytes32;
}
