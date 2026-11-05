// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

/// This file contains EVM unit tests for EIP-7702 "Set EOA account code"
/// https://eips.ethereum.org/EIPS/eip-7702

#include "evm_fixture.hpp"
#include <evmone/delegation.hpp>

using namespace evmc::literals;
using namespace evmone::test;

namespace
{
constexpr auto callee = 0xca11ee_address;
constexpr auto delegate = 0xde1e_address;

/// The delegation designator: the magic followed by the delegate address, 23 bytes.
const bytes designator = bytes{0xef, 0x01, 0x00} + bytes{delegate.bytes, sizeof(delegate.bytes)};
}  // namespace

TEST_P(evm, eip7702_call_designator)
{
    rev = EVMC_PRAGUE;
    ASSERT_EQ(designator.size(), 23);
    host.accounts[callee].code = designator;

    execute(call(callee).gas(50'000));
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls[0];
    EXPECT_EQ(call_msg.recipient, callee);
    EXPECT_EQ(call_msg.code_address, delegate);
    EXPECT_TRUE(call_msg.flags & EVMC_DELEGATED);
}

TEST_P(evm, eip7702_call_designator_magic_only)
{
    rev = EVMC_PRAGUE;
    host.accounts[callee].code = designator.substr(0, 3);

    execute(call(callee).gas(50'000));
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls[0];
    EXPECT_EQ(call_msg.recipient, callee);
    EXPECT_EQ(call_msg.code_address, callee);
    EXPECT_FALSE(call_msg.flags & EVMC_DELEGATED);
}

TEST_P(evm, eip7702_call_designator_too_short)
{
    rev = EVMC_PRAGUE;
    host.accounts[callee].code = designator.substr(0, 22);

    execute(call(callee).gas(50'000));
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls[0];
    EXPECT_EQ(call_msg.recipient, callee);
    EXPECT_EQ(call_msg.code_address, callee);
    EXPECT_FALSE(call_msg.flags & EVMC_DELEGATED);
}

TEST_P(evm, eip7702_call_designator_too_long)
{
    rev = EVMC_PRAGUE;
    host.accounts[callee].code = designator + bytes{0x00};

    execute(call(callee).gas(50'000));
    EXPECT_STATUS(EVMC_SUCCESS);
    ASSERT_EQ(host.recorded_calls.size(), 1);
    const auto& call_msg = host.recorded_calls[0];
    EXPECT_EQ(call_msg.recipient, callee);
    EXPECT_EQ(call_msg.code_address, callee);
    EXPECT_FALSE(call_msg.flags & EVMC_DELEGATED);
}
