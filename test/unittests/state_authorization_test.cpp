// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/authorization.hpp>

using namespace evmc::literals;
using namespace intx;
using namespace evmone;

namespace
{
/// A valid authorization from the execution-specs.
constexpr state::Authorization SIGNED_AUTHORIZATION{
    .chain_id = 0,
    .addr = 0x37f536464af59c8d7358cae965f92cbeadd58dcb_address,
    .nonce = 0,
    .y_parity = 0,
    .r = 0x16ee2526c737c019c381de001f6aa6fb8a5f4090084b2c58f824bc78b00d827f_u256,
    .s = 0x49c6df445a9967a8510b8169445d6d0373b94ad7c789edc23cc8dd2a22fd40c2_u256,
};
}  // namespace

TEST(state_authorization, recover_valid)
{
    EXPECT_EQ(state::recover_authority(SIGNED_AUTHORIZATION),
        0x1ad9bc24818784172ff393bb6f89f094d4d2ca29_address);
}

TEST(state_authorization, recover_rejects_invalid_y_parity)
{
    // y_parity is 0 or 1: the legacy v encodings are not accepted here, nor is any other value.
    auto auth = SIGNED_AUTHORIZATION;
    for (const auto y_parity : {uint8_t{2}, uint8_t{0xff}})
    {
        auth.y_parity = y_parity;
        EXPECT_FALSE(state::recover_authority(auth).has_value()) << int{y_parity};
    }
}
