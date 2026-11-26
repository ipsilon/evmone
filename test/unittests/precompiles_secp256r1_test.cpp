// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/secp256r1.hpp>
#include <gtest/gtest.h>

using namespace evmone::crypto::secp256r1;
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
    {
        // id="u1_eq_u2_and_Q_eq_G"
        0x7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978_u256,
        0x7CF27B188D034F7E8A52380304B51AC3C08969E277F21B35A60B48FC47669978_u256,
        0x830D84E672FCB08275ADC7FCFB4AE53BFC5D90CB2F25834F4DAE81C6B4FC8BD9_u256,
        G.x.value(),
        G.y.value(),
    },
    {
        // Q = −G: the msm() table entry G + Q is the point at infinity.
        0x6038d49a0c9edc92c761aa73983cffd22aab0cfc8e972d09100f2ea5fa094cb0_u256,
        0xe4958eb9a98b86c3fd80ef08a6702a50532d4470a9d5ccd7f21feff38ebfcf06_u256,
        0x2aa1c1dfb80ec2e1ac69e9f321c50304684efc958443a5bfa1e546dd4e6beb86_u256,
        G.x.value(),
        (-G).y.value(),
    },
    {
        // s is 2²¹¹ in Montgomery form, so its inversion starts with 211 halvings.
        0xe4e07c26c991c0d95d8056b8b9b3ee94fd97fcace24780feb1a55bcce5ef83e7_u256,
        0x6bf85d5fe84598b10ca6199ec09ccd7d35dcd7195fdd1dc7737a0c67006ef251_u256,
        0x455770059d208ffa62df7005e277ffffedd356b381e07ad21c9958082f1d0712_u256,
        0x0c901d423c831ca85e27c73c263ba132721bb9d7a84c4f0380b2a6756fd60133_u256,
        0x1c8870234dec878504c174144fa4b14b66a651691606d8173e55bd37e381569e_u256,
    },
    {
        // s in Montgomery form needs 620 safegcd divsteps, the most we found for the order.
        0xfc0f69b129955591d2a768f0b65119837fde68a53be43d4ea2c231a7ca756fba_u256,
        0x6bf85d5fe84598b10ca6199ec09ccd7d35dcd7195fdd1dc7737a0c67006ef251_u256,
        0xb4fc47bcdfbf49002283b7a3330f100284fc6cfd0054fd6f755e686a6db87be0_u256,
        0x0c901d423c831ca85e27c73c263ba132721bb9d7a84c4f0380b2a6756fd60133_u256,
        0x1c8870234dec878504c174144fa4b14b66a651691606d8173e55bd37e381569e_u256,
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
