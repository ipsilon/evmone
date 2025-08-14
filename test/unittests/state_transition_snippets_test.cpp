// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// @file
/// Contains tests for the EVM code snippets: pieces of EVM bytecode used in other places.

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, precompile_proxy)
{
    // Redirects the calldata input to the contract at the address of the callvalue.
    // Then stores the return code and the output in the storage.

    rev = EVMC_PRAGUE;
    static constexpr auto PRECOMPILE_PROXY = 0x707265636f6d70696c652070726f7879_address;

    const auto store_loop_head = 29;
    const auto store_loop_body = 37;
    auto code = bytecode() +                                       //
                OP_PUSH0 + OP_PUSH0 + OP_CALLDATASIZE +            // [input_size, 0, 0]
                OP_DUP1 + OP_PUSH0 + OP_PUSH0 + OP_CALLDATACOPY +  // [input_size, 0, 0]
                OP_PUSH0 + OP_CALLVALUE + OP_GAS +  // [gas, addr, 0, input_size, 0, 0]
                OP_STATICCALL +                     // [return_code]
                sstore(1) +                         // [] store the return code @ 1.
                OP_RETURNDATASIZE +                 // [output_size]
                OP_DUP1 + OP_PUSH0 + OP_PUSH0 + OP_RETURNDATACOPY +  // [output_size]
                OP_PUSH0 + OP_DUP2 + OP_MSTORE +  // [output_size]  clear 32 bytes after the output.
                OP_DUP1 + sstore(2) +             // [output_size]  store the output size @ 2.
                push(32) +                        // [32, output_size]
                OP_PUSH0 +                        // [off=0, 32, output_size]
                OP_JUMPDEST +                     // @store-loop-head
                OP_DUP3 + OP_DUP2 + OP_LT +       // [off < output_size, off, 32, output_size]
                store_loop_body + OP_JUMPI +      // [off, 32, output_size] → @store-loop-body
                OP_STOP +                         //
                OP_JUMPDEST +                     // @store-loop-body
                OP_DUP1 + OP_MLOAD +              // [output[off], off, 32, output_size]
                OP_DUP2 + OP_SSTORE +             // [off, 32, output_size] store output[off] @ off.
                OP_DUP2 + OP_ADD +                // [off+=32, 32, output_size]
                store_loop_head + OP_JUMP;        // [off, 32, output_size] → @store-loop-head

    EXPECT_EQ(code.find(OP_JUMPDEST), store_loop_head);
    EXPECT_EQ(code.find(OP_JUMPDEST, store_loop_head + 1), store_loop_body);

    pre[PRECOMPILE_PROXY] = {.code = code};
    pre[Sender] = {.balance = 10'000'000'000};
    tx.to = PRECOMPILE_PROXY;
    tx.nonce = 0;
    tx.value = 5;
    tx.data = bytes(213, 0);
    tx.data[31] = 1;
    tx.data[63] = 1;
    tx.data[95] = 100;
    tx.data[96] = 17;
    tx.data[97] = 0xff;
    tx.data[101] = 1;

    expect.post[PRECOMPILE_PROXY].storage[0x01_bytes32] = 0x01_bytes32;
    expect.post[PRECOMPILE_PROXY].storage[0x02_bytes32] = 0x64_bytes32;

    expect.post[PRECOMPILE_PROXY].storage[0x00_bytes32] =
        0x00000000c8e4a8fcde71481761e3f9dff38755b7701ea66ee12a392a2bb8e211_bytes32;
    expect.post[PRECOMPILE_PROXY].storage[0x20_bytes32] =
        0x6ca83dbae17957bb3de73ae80f68c59293df4bdcccc6a7d280c56c0bfb2218a5_bytes32;
    expect.post[PRECOMPILE_PROXY].storage[0x40_bytes32] =
        0x438c174b51dcaf20972674a3057273567b0cdcf9f3b88e058dc164a023343343_bytes32;
    expect.post[PRECOMPILE_PROXY].storage[0x60_bytes32] =
        0x5b7780f100000000000000000000000000000000000000000000000000000000_bytes32;
}

TEST_F(state_transition, delegatecall_proxy)
{
    // Takes the address (2 bytes) and the gas limit (4 bytes) from the stack.
    // Then copies whole calldata to memory and DELEGATECALLs to the address with the gas limit.
    // Finally, the call result and the hashed returndata is put in the storage.

    static constexpr auto MAIN_CODE = 0xe1000000000000000000000000000000c0de0000_address;

    auto code = bytecode() +                                 //
                OP_PUSH2 + "0000" + OP_PUSH4 + "00000000" +  // [gas, addr]
                calldatacopy(0, 0, calldatasize()) +         // [gas, addr]
                calldatasize() + push(0) +                   // [0, input_size, gas, addr]
                OP_DUP1 + OP_DUP3 +               // [input_size, 0, 0, input_size, gas, addr]
                add(32) +                         // [output_size, 0, 0, input_size, gas, addr]
                OP_SWAP5 +                        // [addr, 0, 0, input_size, gas, output_size]
                OP_SWAP1 +                        // [0, addr, 0, input_size, gas, output_size]
                OP_SWAP4 +                        // [gas, addr, 0, input_size, 0, output_size]
                OP_DELEGATECALL +                 // [result]
                sstore(1) +                       // []
                keccak256(0, returndatasize()) +  // [keccak(returndata)
                sstore(2);

    EXPECT_EQ(
        hex(code), "61000063000000003660006000373660008082602001949093f46001553d600020600255");

    code[2] = 0x03;  // set the precompile address
    code[6] = 0xff;  // set call gas

    pre[MAIN_CODE] = {.code = code};
    pre[Sender] = {.balance = 10'000'000'000};
    tx.to = MAIN_CODE;
    tx.nonce = 0;
    tx.data = bytes{'a', 'b', 'c'};

    expect.post[MAIN_CODE].storage[0x01_bytes32] = 0x01_bytes32;

    // keccack256(0000000000000000000000008eb208f7e05d987a9b044a8e98c6b087f15a0bfc)
    // which is ripemd160("abc")
    expect.post[MAIN_CODE].storage[0x02_bytes32] =
        0x592a51638107489045cfa238beadd9f3d5cb8c97da92d72d854fd683783f08f2_bytes32;
}
