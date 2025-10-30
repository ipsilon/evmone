// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2022 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// Tests checking if in-development instructions remain undefined before activation.

#include "evm_fixture.hpp"

using namespace evmone::test;

TEST_P(evm, dupn_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(push(1) + "e6" + "00");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, swapn_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(push(1) + push(2) + "e7" + "00");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, exchange_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(push(1) + push(2) + push(3) + "e8" + "00");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, rjump_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(bytecode{"e0"} + "0001" + OP_INVALID + mstore8(0, 1) + ret(0, 1));
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, rjumpi_undefined)
{
    rev = EVMC_MAX_REVISION;
    const auto code =
        push(1) + "e1" + "000a" + mstore8(0, 2) + ret(0, 1) + mstore8(0, 1) + ret(0, 1);
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, rjumpv_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(calldataload(0) + "e2" + "000000" + OP_STOP);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, callf_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(bytecode{"e3"} + "0001" + OP_STOP);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, retf_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(bytecode{"e4"});
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, jumpf_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(bytecode{"e5"} + "0001");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, returndataload_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(staticcall(0) + push(0) + "f7");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, extcall_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(4 * push(0) + "f8");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, extdelegatecall_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(3 * push(0) + "f9");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, extstaticcall_undefined)
{
    rev = EVMC_MAX_REVISION;
    execute(3 * push(0) + "fb");
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, eofcreate_undefined)
{
    rev = EVMC_MAX_REVISION;
    const auto code = calldatacopy(0, 0, OP_CALLDATASIZE) + push(0) + OP_CALLDATASIZE + push(0) +
                      push(0xff) + "ec" + "00" + ret_top();
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}

TEST_P(evm, returncode_undefined)
{
    rev = EVMC_MAX_REVISION;
    const auto code = calldatacopy(0, 0, OP_CALLDATASIZE) + OP_CALLDATASIZE + 0 + "ee" + "00";
    execute(code);
    EXPECT_STATUS(EVMC_UNDEFINED_INSTRUCTION);
}
