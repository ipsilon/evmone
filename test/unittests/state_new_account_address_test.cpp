// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/state/host.hpp>
#include <test/utils/rlp.hpp>

using namespace evmc;
using namespace evmc::literals;

TEST(state_new_account_address, create_examples)
{
    static constexpr auto addr = evmone::state::compute_create_address;

    static constexpr auto S0 = 0x00_address;
    EXPECT_EQ(addr(S0, 0), 0xbd770416a3345f91e4b34576cb804a576fa48eb1_address);
    EXPECT_EQ(addr(S0, 0xffffffffffffffff), 0x1262d73ea59d3a661bf8751d16cf1a5377149e75_address);

    static constexpr auto S1 = 0x01_address;
    EXPECT_EQ(addr(S1, 0), 0x522b3294e6d06aa25ad0f1b8891242e335d3b459_address);
    EXPECT_EQ(addr(S1, 1), 0x535b3d7a252fa034ed71f0c53ec0c6f784cb64e1_address);
    EXPECT_EQ(addr(S1, 0x80), 0x09c1ef8f55c61b94e8b92a55d0891d408a991e18_address);
    EXPECT_EQ(addr(S1, 0xffffffffffffffff), 0x001567239734aeadea21023c2a7c0d9bb9ae4af9_address);

    static constexpr auto S2 = 0x8000000000000000000000000000000000000000_address;
    EXPECT_EQ(addr(S2, 0), 0x3cb1045aee4a06f522ea2b69e4f3d21ed3c135d1_address);
    EXPECT_EQ(addr(S2, 0xffffffffffffffff), 0xe1aa03e4a7b6991d69aff8ece53ceafdf347082e_address);

    const auto beacon_roots1 = addr(0xb20a608c624Ca5003905aA834De7156C68b2E1d0_address, 0);
    EXPECT_EQ(beacon_roots1, 0x00000000219ab540356cbb839cbe05303d7705fa_address);

    const auto beacon_roots2 = addr(0x0B799C86a49DEeb90402691F1041aa3AF2d3C875_address, 0);
    EXPECT_EQ(beacon_roots2, 0x000F3df6D732807Ef1319fB7B8bB8522d0Beac02_address);
}

TEST(state_new_account_address, create_nonces)
{
    // Explore nonce values from all ranges giving RLP encoding schemes.
    static constexpr auto addr = evmone::state::compute_create_address;

    struct TestCase
    {
        uint64_t nonce = 0;
        address expected_addr;
    };
    static constexpr TestCase TEST_CASES[]{
        {0x00, 0xbd770416a3345f91e4b34576cb804a576fa48eb1_address},
        {0x01, 0x5a443704dd4b594b382c22a083e2bd3090a6fef3_address},
        {0x7f, 0x5a1bfc20f2037f3e54d367a70957a5327130cea5_address},
        {0x80, 0xc1784bd8a0ffebd60d0bc7099dcd811b57f30bc4_address},
        {0x81, 0x2823552581b0be905c3d9ba0eb7902a92ccfcf6b_address},
        {0xff, 0x2e021f429ff10bfc9373f73720a14bee2cfd5fdd_address},
        {0x100, 0x1183a5a83c1fa113618603abc4509077ec672699_address},
        {0x102, 0x62ee87c550024e18d0e4686b17239b107e62ec14_address},
        {0xffff, 0xae80be2f887b0efb148934160afd38459969571a_address},
        {0x10000, 0x3c61d75af3a48777914e865f50a38540a11c41c0_address},
        {0x10203, 0x20d47d9d7d3758fd2d8c6cb806bd747325aa5007_address},
        {0xffffff, 0xbbaeb4cb1f1468d2820259d137e7f2a80c751f33_address},
        {0x1000000, 0xb5987b13b2788f3bd5703fd8873557ccada84bb8_address},
        {0x1020304, 0x1f6e1417f5d7ec4f848288984671e7a3570054fb_address},
        {0xffffffff, 0x83317d2df02af8fe91040765f49719e8115c0f04_address},
        {0x100000000, 0x736fd6c74b4cf6cc32253372850bd559067ac5f7_address},
        {0x102030405, 0x05c57791ff9b81d62f55ef655605fe2dfa39ff36_address},
        {0xffffffffff, 0xb07df933f16bfa5a78a4e62826e18cc8acefddb5_address},
        {0x10000000000, 0xcc8d3e72cf698064b521d663088943001a02316f_address},
        {0x10203040506, 0x8598e018febfee476bd1de54be4297e415a723e4_address},
        {0xffffffffffff, 0x154238be5817b2576267644878b50d61f4d240d5_address},
        {0x1000000000000, 0x0ea0057ebcbf62c4021299d808472714b6a0f340_address},
        {0x1020304050607, 0x7f566e72ca7338eb0f695705b187efdb38bf74db_address},
        {0xffffffffffffff, 0x06ef26aa0739f263e6026ec283df7ee579dd05f6_address},
        {0x100000000000000, 0xe72a12bd4ead3c02e618af2cc3379bcddbb56177_address},
        {0x102030405060708, 0x1c70eb0bba02ae69b1c09f3d42da8788513f7de9_address},
        {0xffffffffffffffff, 0x1262d73ea59d3a661bf8751d16cf1a5377149e75_address},
    };

    static constexpr auto SENDER = 0x00_address;  // Use the simplest address.
    for (const auto& [nonce, expected_addr] : TEST_CASES)
    {
        EXPECT_EQ(addr(SENDER, nonce), expected_addr) << std::hex << nonce;
    }
}

TEST(state_new_account_address, create_rlp)
{
    // Compute the RLP payload for keccak256 hash producing the final CREATE address.
    // This test is to visualize what RLP inputs are reaching the final keccak256 hash.
    static constexpr auto rlp = [](const address& addr, uint64_t nonce) {
        return evmc::hex(evmone::rlp::encode_tuple(addr, nonce));
    };

    // The address does not matter for length so use a fixed one.
    static constexpr auto S = 0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa_address;
#define S_RLP "94aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

    EXPECT_EQ(rlp(S, 0x00).length() / 2, 23u);
    EXPECT_EQ(rlp(S, 0x00), "d6" S_RLP "80");
    EXPECT_EQ(rlp(S, 0x01), "d6" S_RLP "01");
    EXPECT_EQ(rlp(S, 0x7f), "d6" S_RLP "7f");
    EXPECT_EQ(rlp(S, 0x80), "d7" S_RLP "8180");
    EXPECT_EQ(rlp(S, 0x81), "d7" S_RLP "8181");
    EXPECT_EQ(rlp(S, 0xff), "d7" S_RLP "81ff");
    EXPECT_EQ(rlp(S, 0x100), "d8" S_RLP "820100");
    EXPECT_EQ(rlp(S, 0x101), "d8" S_RLP "820101");
    EXPECT_EQ(rlp(S, 0xffff), "d8" S_RLP "82ffff");
    EXPECT_EQ(rlp(S, 0x10000), "d9" S_RLP "83010000");
    EXPECT_EQ(rlp(S, 0xffffff), "d9" S_RLP "83ffffff");
    EXPECT_EQ(rlp(S, 0x1000000), "da" S_RLP "8401000000");
    EXPECT_EQ(rlp(S, 0xffffffff), "da" S_RLP "84ffffffff");
    EXPECT_EQ(rlp(S, 0x100000000), "db" S_RLP "850100000000");
    EXPECT_EQ(rlp(S, 0xffffffffff), "db" S_RLP "85ffffffffff");
    EXPECT_EQ(rlp(S, 0x10000000000), "dc" S_RLP "86010000000000");
    EXPECT_EQ(rlp(S, 0xffffffffffff), "dc" S_RLP "86ffffffffffff");
    EXPECT_EQ(rlp(S, 0x1000000000000), "dd" S_RLP "8701000000000000");
    EXPECT_EQ(rlp(S, 0xffffffffffffff), "dd" S_RLP "87ffffffffffffff");
    EXPECT_EQ(rlp(S, 0x100000000000000), "de" S_RLP "880100000000000000");
    EXPECT_EQ(rlp(S, 0xffffffffffffffff), "de" S_RLP "88ffffffffffffffff");
    EXPECT_EQ(rlp(S, 0xffffffffffffffff).length() / 2, 31u);
}

TEST(state_new_account_address, create2)
{
    static constexpr auto addr = evmone::state::compute_create2_address;
    static constexpr address SENDERS[] = {
        0x00_address, 0x01_address, 0x8000000000000000000000000000000000000000_address};
    static constexpr auto z0 = 0x00_bytes32;
    static constexpr auto z1 =
        0xe75fb554e433e03763a1560646ee22dcb74e5274b34c5ad644e7c0f619a7e1d0_bytes32;
    const auto i0 = bytes{};
    const auto i1 = bytes{0xFE};

    EXPECT_EQ(addr(SENDERS[0], z0, i0), 0xe33c0c7f7df4809055c3eba6c09cfe4baf1bd9e0_address);
    EXPECT_EQ(addr(SENDERS[2], z0, i1), 0x3517dea701ed18fc4a99dc111c5946e1f1541dad_address);
    EXPECT_EQ(addr(SENDERS[1], z1, i0), 0x7be1c1cb3b8298f21c56add66defce03e2d32604_address);
    EXPECT_EQ(addr(SENDERS[2], z1, i1), 0x8f459e65c8f00a9c0c0493de7b0c61c3c27f7384_address);
}
