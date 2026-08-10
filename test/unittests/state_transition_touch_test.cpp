// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "../utils/bytecode.hpp"
#include "state_transition.hpp"

using namespace evmc::literals;
using namespace evmone::test;

TEST_F(state_transition, touch_empty_sd)
{
    rev = EVMC_SPURIOUS_DRAGON;  // touching enabled
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY)};
    pre[EMPTY] = {};

    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = false;
}

TEST_F(state_transition, touch_empty_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY)};
    pre[EMPTY] = {};

    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = true;
}

TEST_F(state_transition, touch_nonexistent_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto NONEXISTENT = 0x4e_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(NONEXISTENT)};

    expect.post[*tx.to].exists = true;
    expect.post[NONEXISTENT].exists = true;
}

TEST_F(state_transition, touch_nonexistent_sd)
{
    rev = EVMC_SPURIOUS_DRAGON;
    block.base_fee = 0;
    static constexpr auto NONEXISTENT = 0x4e_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(NONEXISTENT)};

    expect.post[*tx.to].exists = true;
}

TEST_F(state_transition, touch_nonempty_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto WITH_BALANCE = 0xba_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(WITH_BALANCE)};
    pre[WITH_BALANCE] = {.balance = 1};

    expect.post[*tx.to].exists = true;
    expect.post[WITH_BALANCE].exists = true;
}

TEST_F(state_transition, touch_revert_empty)
{
    rev = EVMC_ISTANBUL;  // avoid handling account access (Berlin)
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY) + revert(0, 0)};
    pre[EMPTY] = {};

    expect.status = EVMC_REVERT;
    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = true;
}

TEST_F(state_transition, touch_revert_nonexistent_istanbul)
{
    rev = EVMC_ISTANBUL;  // avoid handling account access (Berlin)
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY) + revert(0, 0)};

    expect.status = EVMC_REVERT;
    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = false;
}

TEST_F(state_transition, touch_revert_nonexistent_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY) + OP_INVALID};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = false;
}

TEST_F(state_transition, touch_revert_nonempty_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto WITH_BALANCE = 0xba_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(WITH_BALANCE) + OP_INVALID};
    pre[WITH_BALANCE] = {.balance = 1};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].exists = true;
    expect.post[WITH_BALANCE].exists = true;
}

TEST_F(state_transition, touch_revert_nonexistent_touch_again_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;
    static constexpr auto REVERT_PROXY = 0x94_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[REVERT_PROXY] = {.code = call(EMPTY) + OP_INVALID};
    pre[*tx.to] = {.code = call(REVERT_PROXY).gas(0xffff) + call(EMPTY)};

    expect.post[*tx.to].exists = true;
    expect.post[REVERT_PROXY].exists = true;
    expect.post[EMPTY].exists = true;
}

TEST_F(state_transition, touch_touch_revert_nonexistent_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;
    static constexpr auto REVERT_PROXY = 0x94_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[REVERT_PROXY] = {.code = call(EMPTY) + OP_INVALID};
    pre[*tx.to] = {.code = call(EMPTY) + call(REVERT_PROXY).gas(0xffff)};

    expect.post[*tx.to].exists = true;
    expect.post[REVERT_PROXY].exists = true;
    expect.post[EMPTY].exists = true;
}

TEST_F(state_transition, touch_revert_touch_revert_nonexistent_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;
    static constexpr auto REVERT_PROXY = 0x94_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[REVERT_PROXY] = {.code = call(EMPTY) + OP_INVALID};
    pre[*tx.to] = {.code = 2 * call(REVERT_PROXY).gas(0xffff)};

    expect.post[*tx.to].exists = true;
    expect.post[REVERT_PROXY].exists = true;
    expect.post[EMPTY].exists = false;
}

TEST_F(state_transition, touch_touch_revert_nonexistent_tw_2)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto EMPTY = 0xee_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(EMPTY) + call(EMPTY) + OP_INVALID};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].exists = true;
    expect.post[EMPTY].exists = false;
}

TEST_F(state_transition, touch_revert_selfdestruct_to_nonexistient_tw)
{
    rev = EVMC_TANGERINE_WHISTLE;  // no touching
    block.base_fee = 0;
    static constexpr auto DESTRUCTOR = 0xde_address;
    static constexpr auto BENEFICIARY = 0xbe_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(DESTRUCTOR).gas(0xffff) + OP_INVALID};
    pre[DESTRUCTOR] = {.code = selfdestruct(BENEFICIARY)};

    expect.status = EVMC_INVALID_INSTRUCTION;
    expect.post[*tx.to].exists = true;
    expect.post[DESTRUCTOR].exists = true;
    expect.post[BENEFICIARY].exists = false;
}

TEST_F(state_transition, touch_revert_ripemd_frontier)
{
    // Before Spurious Dragon the 0x03 quirk is off: the failed call's touch of 0x03 is reverted and
    // 0x03 does not linger. Guards the >= EVMC_SPURIOUS_DRAGON lower bound.
    rev = EVMC_FRONTIER;
    block.base_fee = 0;
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(0x03_address)};  // gas = 0 -> RIPEMD out-of-gas -> failed call

    expect.post[*tx.to].exists = true;
    expect.post[0x03_address].exists = false;
}

TEST_F(state_transition, touch_revert_ripemd_london)
{
    // In range the quirk keeps the touch, so a pre-existing empty 0x03 leaf is swept by EIP-161
    // even though the touching call reverted. Guards that the quirk stays active up to the Merge.
    rev = EVMC_LONDON;
    block.base_fee = 0;
    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(0x03_address)};  // gas = 0 -> RIPEMD out-of-gas -> failed call
    pre[0x03_address] = {};                      // pre-existing empty leaf

    expect.post[*tx.to].exists = true;
    expect.post[0x03_address].exists = false;  // deleted by the retained touch
}

// A storage-only account (nonce 0, balance 0, no code) is empty per EIP-158, so it is eligible for
// the end-of-tx sweep, but only a genuine touch may trigger it. Constructing one in the pre-state
// needs a fork before EIP-7523.

TEST_F(state_transition, touch_access_list_storage_only)
{
    // Warming via the access list is not a touch.
    rev = EVMC_LONDON;
    static constexpr auto STORAGE_ONLY = 0x5a_address;

    tx.to = To;
    tx.access_list = {{STORAGE_ONLY, {}}};
    pre[*tx.to] = {.code = bytecode{OP_STOP}};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.post[*tx.to].exists = true;
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, touch_balance_storage_only)
{
    // BALANCE loads the account, where the access list above only warms it.
    rev = EVMC_LONDON;
    static constexpr auto STORAGE_ONLY = 0x5a_address;

    tx.to = To;
    pre[*tx.to] = {.code = push(STORAGE_ONLY) + OP_BALANCE + OP_POP};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.post[*tx.to].exists = true;
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}

TEST_F(state_transition, touch_revert_storage_only)
{
    // The rollback must undo the touched flag, or the account is swept despite the revert.
    rev = EVMC_ISTANBUL;  // Berlin's account access would journal the flag on its own.
    block.base_fee = 0;
    static constexpr auto STORAGE_ONLY = 0x5a_address;

    tx.type = Transaction::Type::legacy;
    tx.to = To;
    pre[*tx.to] = {.code = call(STORAGE_ONLY) + revert(0, 0)};
    pre[STORAGE_ONLY] = {.storage = {{0x01_bytes32, 0x01_bytes32}}};

    expect.status = EVMC_REVERT;
    expect.post[*tx.to].exists = true;
    expect.post[STORAGE_ONLY].exists = true;
    expect.post[STORAGE_ONLY].storage[0x01_bytes32] = 0x01_bytes32;
}
