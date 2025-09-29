// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/secp256r1.hpp>
#include <gtest/gtest.h>

using namespace evmmax::secp256r1;
using namespace intx;

namespace
{
struct P256VerifyInput
{
    uint256 h;
    uint256 r;
    uint256 s;
    uint256 x;
    uint256 y;
};

const P256VerifyInput VALID_INPUTS[]{
    {
        0xbb5a52f42f9c9261ed4361f59422a1e30036e7c32b270c8807a419feca605023_u256,
        0x2ba3a8be6b94d5ec80a6d9d1190a436effe50d85a1eee859b8cc6af9bd5c2e18_u256,
        0x4cd60b855d442f5b3c7b11eb6c4e0ae7525fe710fab9aa7c77a67f79e6fadd76_u256,
        0x2927b10512bae3eddcfe467828128bad2903269919f7086069c8c4df6c732838_u256,
        0xc7787964eaac00e5921fb1498a60f4606766b3d9685001558d1a974e7341513e_u256,
    },
    {
        // Valid public key with 0 x-coordinate.
        0xc3d3be9eb3577f217ae0ab360529a30b18adc751aec886328593d7d6fe042809_u256,
        0x3a4e97b44cbf88b90e6205a45ba957e520f63f3c6072b53c244653278a1819d8_u256,
        0x6a184aa037688a5ebd25081fd2c0b10bb64fa558b671bd81955ca86e09d9d722_u256,
        0,
        0x66485c780e2f83d72433bd5d84a06bb6541c2af31dae871728bf856a174f93f4_u256,
    },
};

const P256VerifyInput INVALID_INPUTS[]{
    {0, 0, 0, 0, 0},
};
}  // namespace

TEST(secp256r1, valid)
{
    for (size_t i = 0; i < std::size(VALID_INPUTS); ++i)
    {
        const auto& [h_int, r, s, x, y] = VALID_INPUTS[i];
        ethash::hash256 h{};
        be::store(h.bytes, h_int);
        const auto result = verify(h, r, s, x, y);
        EXPECT_TRUE(result) << i;
    }
}

TEST(secp256r1, invalid)
{
    for (size_t i = 0; i < std::size(INVALID_INPUTS); ++i)
    {
        const auto& [h_int, r, s, x, y] = INVALID_INPUTS[i];
        ethash::hash256 h{};
        be::store(h.bytes, h_int);
        const auto result = verify(h, r, s, x, y);
        EXPECT_FALSE(result) << i;
    }
}
