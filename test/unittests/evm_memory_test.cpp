// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evm_fixture.hpp"
#include <evmone/instructions_traits.hpp>
#include <algorithm>

using namespace evmone;
using namespace evmc::literals;
using namespace intx;
using namespace evmone::test;

TEST_P(evm, memory_and_not)
{
    execute(42, push(0) + push(1) + OP_DUP1 + OP_NOT + OP_DUP2 + OP_MSTORE8 + OP_DUP2 + OP_MLOAD +
                    OP_DUP3 + OP_MSTORE + OP_DUP1 + OP_ADD + OP_SWAP1 + OP_RETURN);
    EXPECT_GAS_USED(EVMC_SUCCESS, 42);
    ASSERT_EQ(result.output_size, 2);
    EXPECT_EQ(result.output_data[1], 0xfe);
    EXPECT_EQ(result.output_data[0], 0);
}

TEST_P(evm, msize)
{
    execute(29, "60aa6022535960005360016000f3");
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(result.gas_left, 0);
    ASSERT_EQ(result.output_size, 1);
    EXPECT_EQ(result.output_data[0], 0x40);
}

TEST_P(evm, calldatacopy)
{
    bytecode s;
    s += "366001600037";  // CALLDATASIZE 1 0 CALLDATACOPY
    s += "600a6000f3";    // RETURN(0,10)
    execute(s, "0102030405"_hex);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 23);
    ASSERT_EQ(result.output_size, 10);
    EXPECT_EQ(bytes_view(&result.output_data[0], 10), "02030405000000000000"_hex);

    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    EXPECT_EQ(gas_used, 20);

    execute(bytecode{"60ff66fffffffffffffa60003760ff6000f3"});
    EXPECT_EQ(gas_used, 66);
    ASSERT_EQ(result.output_size, 0xff);
    EXPECT_EQ(std::count(result.output_data, result.output_data + result.output_size, 0), 0xff);
}

TEST_P(evm, memory_big_allocation)
{
    constexpr auto size = 256 * 1024 + 1;
    const auto code = ret(0, size);
    execute(code);
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, size);
    for (auto b : bytes_view{result.output_data, result.output_size})
        EXPECT_EQ(b, 0);
}

TEST_P(evm, memory_grow_mstore8)
{
    const auto code = calldataload(0) + push(0) + OP_JUMPDEST + mstore8(OP_DUP1, OP_DUP1) + add(1) +
                      jumpi(5, iszero(eq(OP_DUP3, OP_DUP1))) + ret(0, OP_MSIZE);

    constexpr size_t size = 4 * 1024 + 256 + 1;

    execute(code, evmc::bytes32{size});
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, ((size + 31) / 32) * 32);

    for (size_t i = 0; i < size; ++i)
        EXPECT_EQ(result.output_data[i], i % 256);

    for (size_t i = size; i < result.output_size; ++i)
        EXPECT_EQ(result.output_data[i], 0);
}

TEST_P(evm, mstore8_memory_cost)
{
    auto code = push(0) + mstore8(0);
    execute(12, code);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    execute(11, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, keccak256_memory_cost)
{
    execute(45, keccak256(0, 1));
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    execute(44, keccak256(0, 1));
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}

TEST_P(evm, calldatacopy_memory_cost)
{
    const auto code = calldatacopy(0, 0, 1);
    execute(18, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 18);
}


struct memory_access_opcode
{
    Opcode opcode;
    int memory_index_arg;
    int memory_size_arg;
};


static constexpr memory_access_opcode memory_access_opcodes[] = {
    {OP_KECCAK256, 0, 1},
    {OP_CALLDATACOPY, 0, 2},
    {OP_CODECOPY, 0, 2},
    {OP_MLOAD, 0, -1},
    {OP_MSTORE, 0, -1},
    {OP_MSTORE8, 0, -1},
    {OP_MCOPY, 0, 2},
    {OP_MCOPY, 1, 2},
    {OP_EXTCODECOPY, 1, 3},
    {OP_RETURNDATACOPY, 0, 2},
    {OP_LOG0, 0, 1},
    {OP_LOG1, 0, 1},
    {OP_LOG2, 0, 1},
    {OP_LOG3, 0, 1},
    {OP_LOG4, 0, 1},
    {OP_RETURN, 0, 1},
    {OP_REVERT, 0, 1},
    {OP_CALL, 3, 4},
    {OP_CALL, 5, 6},
    {OP_CALLCODE, 3, 4},
    {OP_CALLCODE, 5, 6},
    {OP_DELEGATECALL, 2, 3},
    {OP_DELEGATECALL, 4, 5},
    {OP_STATICCALL, 2, 3},
    {OP_STATICCALL, 4, 5},
    {OP_CREATE, 1, 2},
    {OP_CREATE2, 1, 2},
};

struct
{
    uint256 index;
    uint256 size;
} static constexpr memory_access_test_cases[] = {
    {0, 0x100000000},
    {0, 0x10000000000000000_u256},
    {0, 0x100000000000000000000000000000000_u256},
    {0, 0x1000000000000000000000000000000000000000000000000_u256},
    {0, 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256},
    {0x80000000, 0x80000000},
    {0xffffffe0, 32},
    {0xffffffff, 1},
    {0x100000000, 0},
    {0x100000000, 1},
    {0x100000000, 0x100000000},
    {0xffffffffffffffc1, 32},
    {0xffffffffffffffc0, 32},
    {0xffffffffffffffdf, 32},
    {0xffffffffffffffe0, 32},
    {0xffffffffffffffff, 1},
    {0x80000000000000000000000000000000_u256, 1},
    {0x800000000000000000000000000000000000000000000000_u256, 1},
    {0x8000000000000000000000000000000000000000000000000000000000000000_u256, 1},
    {0x8000000000000000800000000000000080000000000000000000000000000000_u256, 1},
    {0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256, 1},
    {0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256,
        0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256},
};

TEST_P(evm, memory_access)
{
    rev = EVMC_EXPERIMENTAL;
    // This test checks if instructions accessing memory properly respond with out-of-gas
    // error for combinations of memory offset and memory size arguments.

    for (const auto& p : memory_access_test_cases)
    {
        for (auto& t : memory_access_opcodes)
        {
            const auto num_args = int{instr::traits[t.opcode].stack_height_required};
            auto h = std::max(num_args, t.memory_size_arg + 1);
            auto code = bytecode{};

            if (t.memory_size_arg >= 0)
            {
                while (--h != t.memory_size_arg)
                    code += push(0);

                code += push(p.size);
            }
            else if (p.index == 0 || p.size == 0)
                continue;  // Skip opcodes not having SIZE argument.

            while (--h != t.memory_index_arg)
                code += push(0);

            code += push(p.index);

            while (h-- != 0)
                code += push(0);

            code += bytecode{t.opcode};

            const auto gas = 8796294610952;
            execute(gas, code);

            auto case_descr_str = std::ostringstream{};
            case_descr_str << "offset = 0x" << to_string(p.index, 16) << " size = 0x"
                           << to_string(p.size, 16) << " opcode " << instr::traits[t.opcode].name;
            const auto case_descr = case_descr_str.str();

            if (p.size == 0)  // It is allowed to request 0 size memory at very big offset.
            {
                EXPECT_EQ(result.status_code, (t.opcode == OP_REVERT) ? EVMC_REVERT : EVMC_SUCCESS)
                    << case_descr;
                EXPECT_NE(result.gas_left, 0) << case_descr;
            }
            else
            {
                if (t.opcode == OP_RETURNDATACOPY)
                {
                    // In case of RETURNDATACOPY the "invalid memory access" might also be returned.
                    EXPECT_TRUE(result.status_code == EVMC_OUT_OF_GAS ||
                                result.status_code == EVMC_INVALID_MEMORY_ACCESS);
                }
                else
                {
                    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS) << case_descr;
                }

                EXPECT_EQ(result.gas_left, 0) << case_descr;
            }
        }
    }
}

TEST_P(evm, mcopy)
{
    rev = EVMC_CANCUN;
    bytecode s;
    s += mstore(0, push(0x0123456789abcdef000000000000000000000000000000000000000000000000_u256)) +
         mcopy(32, 0, 8) + ret(32, 8);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 8);
    EXPECT_EQ(bytes_view(&result.output_data[0], 8), "0123456789abcdef"_hex);

    // copy from uninitialized memory
    s = mcopy(0, 24, 16) + ret(0, 16);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 16);
    EXPECT_EQ(bytes_view(&result.output_data[0], 16), "00000000000000000000000000000000"_hex);

    // copy from initialized + uninitialized memory
    s = mstore(0, push(0x0000000000000000000000000000000000000000000000000123456789abcdef_u256)) +
        mcopy(64, 24, 16) + ret(64, 16);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 16);
    EXPECT_EQ(bytes_view(&result.output_data[0], 16), "0123456789abcdef0000000000000000"_hex);

    // overlapping src < dst
    s = mstore(0, push(0x0123456789abcdef000000000000000000000000000000000000000000000000_u256)) +
        mcopy(4, 0, 8) + ret(0, 16);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 16);
    EXPECT_EQ(bytes_view(&result.output_data[0], 16), "012345670123456789abcdef00000000"_hex);

    // overlapping src > dst
    s = mstore(0, push(0x00112233445566778899aabbccddeeff00000000000000000000000000000000_u256)) +
        mcopy(0, 4, 8) + ret(0, 16);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 16);
    EXPECT_EQ(bytes_view(&result.output_data[0], 16), "445566778899aabb8899aabbccddeeff"_hex);

    // overlapping src == dst
    s = mstore(0, push(0x00112233445566778899aabbccddeeff00000000000000000000000000000000_u256)) +
        mcopy(4, 4, 8) + ret(0, 16);
    execute(s);
    EXPECT_EQ(result.status_code, EVMC_SUCCESS);
    ASSERT_EQ(result.output_size, 16);
    EXPECT_EQ(bytes_view(&result.output_data[0], 16), "00112233445566778899aabbccddeeff"_hex);
}

TEST_P(evm, mcopy_memory_cost)
{
    rev = EVMC_CANCUN;
    const auto code = mcopy(0, 0, 1);
    execute(18, code);
    EXPECT_GAS_USED(EVMC_SUCCESS, 18);

    execute(17, code);
    EXPECT_EQ(result.status_code, EVMC_OUT_OF_GAS);
}
