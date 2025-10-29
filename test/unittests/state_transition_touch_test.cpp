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
