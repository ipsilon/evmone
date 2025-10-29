// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/blob_schedule.hpp"
#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, block_apply_withdrawal)
{
    static constexpr auto withdrawal_address = 0x8888_address;

    block.withdrawals = {{0, 0, withdrawal_address, 3}};
    tx.to = To;
    expect.post[withdrawal_address].balance = intx::uint256{3} * 1'000'000'000;
}

TEST_F(state_transition, known_block_hash)
{
    block_hashes = {
        {1, 0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_bytes32},
        {2, 0x0000000000000000000000000000000000000000000000000000000000000111_bytes32},
    };
    block.number = 5;

    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = sstore(0, blockhash(1)) + sstore(1, blockhash(2))};
    expect.post[To].storage[0x00_bytes32] =
        0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421_bytes32;
    expect.post[To].storage[0x01_bytes32] =
        0x0000000000000000000000000000000000000000000000000000000000000111_bytes32;
}

TEST_F(state_transition, known_block_hash_fake)
{
    block.number = 2;
    tx.to = To;
    pre[*tx.to] = {.nonce = 1, .code = sstore(0, blockhash(0)) + sstore(1, blockhash(1))};
    expect.post[To].storage[0x00_bytes32] =
        0x044852b2a670ade5407e78fb2863c51de9fcb96542a07186fe3aeda6bb8a116d_bytes32;
    expect.post[To].storage[0x01_bytes32] =
        0xc89efdaa54c0f20c7adf612882df0950f5a951637e0307cdcb4c672f298b8bc6_bytes32;
}

TEST_F(state_transition, block_apply_ommers_reward)
{
    rev = EVMC_LONDON;

    static constexpr auto o1 = Ommer{0x0eeee1_address, 1};
    static constexpr auto o2 = Ommer{0x0eeee2_address, 3};

    // Use high value 5 ETH to catch potential uint64 overflows.
    block_reward = 5'000'000'000'000'000'000;
    block.ommers = {o1, o2};
    tx.to = To;
    expect.post[o1.beneficiary].balance = intx::uint256{block_reward} * (8 - o1.delta) / 8;
    expect.post[o2.beneficiary].balance = intx::uint256{block_reward} * (8 - o2.delta) / 8;

    // Two ommers +1/32 * block_reward for each. +21000 cost of the tx goes to coinbase.
    expect.post[Coinbase].balance = 21000 + intx::uint256{block_reward} + block_reward / 16;
}

TEST_F(state_transition, eip7516_blob_base_fee)
{
    rev = EVMC_CANCUN;

    block.excess_blob_gas = 0xabcd00;
    // 0x1d is the result of ref implementation in EIP-4844
    static constexpr auto price = 0x1d;
    block.blob_base_fee = price;
    assert(state::compute_blob_gas_price(get_blob_params(rev), *block.excess_blob_gas) ==
           *block.blob_base_fee);

    tx.to = To;
    pre[*tx.to] = {.code = sstore(0x4a, OP_BLOBBASEFEE)};

    expect.post[To].storage[0x4a_bytes32] = bytes32(price);
}
