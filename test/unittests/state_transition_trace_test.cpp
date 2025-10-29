// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "state_transition.hpp"
#include <test/utils/bytecode.hpp>

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, trace_example)
{
    tx.to = To;
    pre[To] = {.code = revert(0, 1)};

    expect.status = EVMC_REVERT;
    expect.post[To].exists = true;

    expect.trace = R"(
{"pc":0,"op":96,"gas":"0xef038","gasCost":"0x3","memSize":0,"stack":[],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":2,"op":96,"gas":"0xef035","gasCost":"0x3","memSize":0,"stack":["0x1"],"depth":1,"refund":0,"opName":"PUSH1"}
{"pc":4,"op":253,"gas":"0xef032","gasCost":"0x0","memSize":0,"stack":["0x1","0x0"],"depth":1,"refund":0,"opName":"REVERT"}
)";
}
