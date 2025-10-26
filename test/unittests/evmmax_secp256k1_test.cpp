// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/secp256k1.hpp>
#include <gtest/gtest.h>
#include <test/utils/utils.hpp>

using namespace evmmax::secp256k1;
using namespace evmc::literals;
using namespace evmone::test;

namespace
{
/// Scalar multiplication in secp256k1.
///
/// Computes [c]P for a point in affine coordinate on the secp256k1 curve.
/// Convenience wrapper for point multiplication test.
AffinePoint mul(const AffinePoint& p, const uint256& c) noexcept
{
    const auto r = evmmax::ecc::mul(p, c);
    return evmmax::ecc::to_affine<Curve>(r);
}
}  // namespace

TEST(secp256k1, field_sqrt)
{
    const auto& m = Curve::Fp;

    for (const auto& t : std::array{
             1_u256,
             0x6e140df17432311190232a91a38daed3ee9ed7f038645dd0278da7ca6e497de_u256,
             0xf3b9accc43dc8919ba3b4f1e14c8f7c72e7c4c013a404e9fd35e9c9a5b7b228_u256,
             0x3db99f8c1e729de4c9a283e8714b9f6bc3ef22ac5fd70daaa88b73dcf52ebe9_u256,
             0x37ec7e48f17a78e38d7b3c77d15be8c4a8e6bae83971fdec3b25f861be4b7da_u256,
             0x5b1a739f853ba7e4c6a2f3e91c7b2f7c87d4c0d98ba2fde82a79f3e5d8b76b9_u256,
             0x69187a3b9c5de9e4783a29df87b6f8c5d3a2b6d98c5d7ea1b28f7e5d9a7b6b8_u256,
             0x7a98763a85df9e7c6a28d9f7b6f8d5c3a2b7c6d98c5d7e9a1b2f7d6e5a9b7b6_u256,
             0x8b87953a75ef8d7b5a27c8e7a6f7d4b2a1b6c5d87b5c6d89a0b1e6d4a8a6b5_u256,
             0x9c76942a65df7c6a4a16b7d6a5f6c3a0b0c4b5c76a4b5c78a9f6d3c4a7a5b4_u256,
             0xad65931a55cf6b594915a6c5a4f5b2a9f0b3a4b6593a4b6789e5c2b39694a3_u256,
             0xbe54820a45bf5a48381495b494e4a1f8e9a293b548394a5678d4b1a28583a2_u256,
             Curve::FIELD_PRIME - 1,
         })
    {
        const auto a = m.to_mont(t);
        const auto a2 = m.mul(a, a);
        const auto a2_sqrt = field_sqrt(m, a2);
        ASSERT_TRUE(a2_sqrt.has_value()) << to_string(t);
        EXPECT_TRUE(a2_sqrt == a || a2_sqrt == m.sub(0, a)) << to_string(t);
    }
}

TEST(secp256k1, field_sqrt_invalid)
{
    const auto& m = Curve::Fp;

    for (const auto& t : std::array{3_u256, Curve::FIELD_PRIME - 1})
    {
        EXPECT_FALSE(field_sqrt(m, m.to_mont(t)).has_value());
    }
}

TEST(secp256k1, scalar_inv)
{
    const evmmax::ModArith n{Curve::ORDER};

    for (const auto& t : std::array{
             1_u256,
             0x6e140df17432311190232a91a38daed3ee9ed7f038645dd0278da7ca6e497de_u256,
             Curve::ORDER - 1,
         })
    {
        ASSERT_LT(t, Curve::ORDER);
        const auto a = n.to_mont(t);
        const auto a_inv = n.inv(a);
        const auto p = n.mul(a, a_inv);
        EXPECT_EQ(n.from_mont(p), 1) << hex(t);
    }
}

TEST(secp256k1, calculate_y)
{
    const auto& m = Curve::Fp;

    struct TestCase
    {
        uint256 x;
        uint256 y_even;
        uint256 y_odd;
    };

    const TestCase test_cases[] = {
        {
            1_u256,
            0x4218f20ae6c646b363db68605822fb14264ca8d2587fdd6fbc750d587e76a7ee_u256,
            0xbde70df51939b94c9c24979fa7dd04ebd9b3572da7802290438af2a681895441_u256,
        },
        {
            0xb697546bfbc062d06df1d25a26e4fadfe2f2a48109c349bf65d2b01182f3aa60_u256,
            0xd02714d31d0c08c38037400d232886863b473a37adba9823ea44ae50028a5bea_u256,
            0x2fd8eb2ce2f3f73c7fc8bff2dcd77979c4b8c5c8524567dc15bb51aefd75a045_u256,
        },
        {
            0x18f4057699e2d9679421de8f4e11d7df9fa4b9e7cb841ea48aed75f1567b9731_u256,
            0x6db5b7ecd8e226c06f538d15173267bf1e78acc02bb856e83b3d6daec6a68144_u256,
            0x924a4813271dd93f90ac72eae8cd9840e187533fd447a917c4c2925039597aeb_u256,
        },
    };

    for (const auto& t : test_cases)
    {
        const auto x = m.to_mont(t.x);

        const auto y_even = calculate_y(m, x, false);
        ASSERT_TRUE(y_even.has_value());
        EXPECT_EQ(m.from_mont(*y_even), t.y_even);

        const auto y_odd = calculate_y(m, x, true);
        ASSERT_TRUE(y_odd.has_value());
        EXPECT_EQ(m.from_mont(*y_odd), t.y_odd);
    }
}

TEST(secp256k1, calculate_y_invalid)
{
    const auto& m = Curve::Fp;

    for (const auto& t : std::array{
             0x207ea538f1835f6de40c793fc23d22b14da5a80015a0fecddf56f146b21d7949_u256,
             Curve::FIELD_PRIME - 1,
         })
    {
        const auto x = m.to_mont(t);

        const auto y_even = calculate_y(m, x, false);
        ASSERT_FALSE(y_even.has_value());

        const auto y_odd = calculate_y(m, x, true);
        ASSERT_FALSE(y_odd.has_value());
    }
}

TEST(secp256k1, point_to_address)
{
    // Check if converting the point at infinity gives the known address.
    // https://www.google.com/search?q=0x3f17f1962B36e491b30A40b2405849e597Ba5FB5
    // https://etherscan.io/address/0x3f17f1962b36e491b30a40b2405849e597ba5fb5
    EXPECT_EQ(to_address({}), 0x3f17f1962B36e491b30A40b2405849e597Ba5FB5_address);
}

TEST(evmmax, secp256k1_calculate_u1)
{
    // u1 = -zr^(-1)
    const auto z = 0x31d6fb860f6d12cee6e5b640646089bd5883d586e43de3dedc75695c11ac2da9_u256;
    const auto r = 0x71cd6bfc24665312ff489aba9279710a560eda74aca333bf298785dc3cd72f6e_u256;
    const auto expected = 0xd80ea4db5200c96e969270ab7c105e16abb9fc18a6e01cc99575dd3f5ce41eed_u256;

    const auto& m = Curve::Fp;
    const auto z_mont = m.to_mont(z);
    const auto r_mont = m.to_mont(r);
    const auto r_inv = m.inv(r_mont);
    const auto z_neg = m.sub(0, z_mont);
    const auto u1_mont = m.mul(z_neg, r_inv);
    const auto u1 = m.from_mont(u1_mont);
    EXPECT_EQ(u1, expected);
}

TEST(evmmax, secp256k1_calculate_u2)
{
    // u2 = sr^(-1)
    const auto r = 0x27bc00995393e969525f2d02e731437402aa12a9a09125d1e322d62f05a2b54f_u256;
    const auto s = 0x7ce91fc325f28e78a016fa674a80d85581cc278d15453ea2fede2471b1adaada_u256;
    const auto expected = 0xf888ea06899abc190fa37a165c98e6d4b00b13c50db1d1c34f38f0ab8fd9c29b_u256;

    const auto& m = Curve::Fp;
    const auto s_mont = m.to_mont(s);
    const auto r_mont = m.to_mont(r);
    const auto r_inv = m.inv(r_mont);
    const auto u2_mont = m.mul(s_mont, r_inv);
    const auto u2 = m.from_mont(u2_mont);
    EXPECT_EQ(u2, expected);
}

TEST(evmmax, secp256k1_hash_to_number)
{
    const auto max_h = ~uint256{};
    const auto hm = max_h % Curve::FIELD_PRIME;

    // Optimized mod.
    const auto hm2 = max_h - Curve::FIELD_PRIME;
    EXPECT_EQ(hm2, hm);
}

TEST(evmmax, secp256k1_pt_add_inf)
{
    const AffinePoint p1{0x18f4057699e2d9679421de8f4e11d7df9fa4b9e7cb841ea48aed75f1567b9731_u256,
        0x6db5b7ecd8e226c06f538d15173267bf1e78acc02bb856e83b3d6daec6a68144_u256};
    const AffinePoint inf;
    ASSERT_TRUE(inf == 0);

    EXPECT_EQ(add(p1, inf), p1);
    EXPECT_EQ(add(inf, p1), p1);
    EXPECT_EQ(add(inf, inf), inf);
}

TEST(evmmax, secp256k1_pt_add)
{
    const AffinePoint p1{0x18f4057699e2d9679421de8f4e11d7df9fa4b9e7cb841ea48aed75f1567b9731_u256,
        0x6db5b7ecd8e226c06f538d15173267bf1e78acc02bb856e83b3d6daec6a68144_u256};
    const AffinePoint p2{0xf929e07c83d65da3569113ae03998d13359ba982216285a686f4d66e721a0beb_u256,
        0xb6d73966107b10526e2e140c17f343ee0a373351f2b1408923151b027f55b82_u256};
    const AffinePoint p3{0xf929e07c83d65da3569113ae03998d13359ba982216285a686f4d66e721a0beb_u256,
        0xf4928c699ef84efad91d1ebf3e80cbc11f5c8ccae0d4ebf76dceae4ed80aa0ad_u256};
    const AffinePoint p4{
        0x1_u256, 0xbde70df51939b94c9c24979fa7dd04ebd9b3572da7802290438af2a681895441_u256};

    {
        const AffinePoint e{0x40468d7704db3d11961ab9c222e35919d7e5d1baef59e0f46255d66bec3bd1d3_u256,
            0x6fff88d9f575236b6cc5c74e7d074832a460c2792fba888aea7b9986429dd7f7_u256};
        EXPECT_EQ(add(p1, p2), e);
    }
    {
        const AffinePoint e{0xd8e7b42b8c82e185bf0669ce0754697a6eb46c156497d5d1971bd6a23f38ed9e_u256,
            0x628c3107fc73c92e7b8c534e239257fb2de95bd6b965dc1021f636da086a7e99_u256};
        EXPECT_EQ(add(p1, p1), e);
    }
    {
        const AffinePoint e{0xdf592d726f42759020da10d3106db3880e514c783d6970d2a9085fb16879b37f_u256,
            0x10aa0ef9fe224e3797792b4b286b9f63542d4c11fe26d449a845b9db0f5993f9_u256};
        EXPECT_EQ(add(p1, p3), e);
    }
    {
        const AffinePoint e{0x12a5fd099bcd30e7290e58d63f8d5008287239500e6d0108020040497c5cb9c9_u256,
            0x7f6bd83b5ac46e3b59e24af3bc9bfbb213ed13e21d754e4950ae635961742574_u256};
        EXPECT_EQ(add(p1, p4), e);
    }
}

TEST(evmmax, secp256k1_pt_mul_inf)
{
    const AffinePoint p1{0x18f4057699e2d9679421de8f4e11d7df9fa4b9e7cb841ea48aed75f1567b9731_u256,
        0x6db5b7ecd8e226c06f538d15173267bf1e78acc02bb856e83b3d6daec6a68144_u256};
    const AffinePoint inf;

    EXPECT_EQ(mul(p1, 0), inf);
    EXPECT_EQ(mul(p1, Curve::ORDER), inf);
    EXPECT_EQ(mul(inf, 0), inf);
    EXPECT_EQ(mul(inf, 1), inf);
    EXPECT_EQ(mul(inf, Curve::ORDER - 1), inf);
    EXPECT_EQ(mul(inf, Curve::ORDER), inf);
}

TEST(evmmax, secp256k1_pt_mul)
{
    const AffinePoint p1{0x18f4057699e2d9679421de8f4e11d7df9fa4b9e7cb841ea48aed75f1567b9731_u256,
        0x6db5b7ecd8e226c06f538d15173267bf1e78acc02bb856e83b3d6daec6a68144_u256};

    {
        const auto d{100000000000000000000_u256};
        const AffinePoint e{0x4c34e6dc48badd579d1ce4702fd490fb98fa0e666417bfc2d4ff8e957d99c565_u256,
            0xb53da5be179d80c7f07226ba79b6bce643d89496b37d6bc2d111b009e37cc28b_u256};
        auto r = mul(p1, d);
        EXPECT_EQ(r, e);
    }

    {
        const auto d{100000000000000000000000000000000_u256};
        const AffinePoint e{0xf86902594c8a4e4fc5f6dfb27886784271302c6bab3dc4350a0fe7c5b056af66_u256,
            0xb5748aa8f9122bfdcbf5846f6f8ec76f41626642a3f2ea0f483c92bf915847ad_u256};
        auto r = mul(p1, d);
        EXPECT_EQ(r, e);
    }

    {
        const auto u1 = 0xd17a4c1f283fa5d67656ea81367b520eaa689207e5665620d4f51c7cf85fa220_u256;
        const AffinePoint G{0x79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798_u256,
            0x483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8_u256};
        const AffinePoint e{0x39cb41b2567f68137aae52e99dbe91cd38d9faa3ba6be536a04355b63a7964fe_u256,
            0xf31e6abd08cbd8e4896c9e0304b25000edcd52a9f6d2bac7cfbdad2c835c9a35_u256};
        auto r = mul(G, u1);
        EXPECT_EQ(r, e);
    }
}


struct TestCaseECR
{
    evmc::bytes32 hash;
    uint256 r;
    uint256 s;
    bool parity = false;
    AffinePoint pubkey;
};

static const TestCaseECR test_cases_ecr[] = {
    {
        0x18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c_bytes32,
        0x7af9e73057870458f03c143483bc5fcb6f39d01c9b26d28ed9f3fe23714f6628_u256,
        0x3134a4ba8fafe11b351a720538398a5635e235c0b3258dce19942000731079ec_u256,
        false,
        {
            0x43ec87f8ee6f58605d947dac51b5e4cfe26705f509e5dad058212aadda180835_u256,
            0x90ebad786ce091f5af1719bf30ee236a4e6ce8a7ab6c36a16c93c6177aa109df_u256,
        },
    },
};

TEST(evmmax, ecr)
{
    for (const auto& t : test_cases_ecr)
    {
        const auto h = std::bit_cast<ethash::hash256>(t.hash);
        const auto result = secp256k1_ecdsa_recover(h, t.r, t.s, t.parity);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->x, t.pubkey.x);
        EXPECT_EQ(result->y, t.pubkey.y);
        EXPECT_EQ(*result, t.pubkey);
    }
}


struct TestCaseECRecovery
{
    bytes input;
    bytes expected_output;
};

static const TestCaseECRecovery test_cases[] = {
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001c73b1693892219d736caba55bdb67216e485557ea6b6af75f37096c9aa6a5a75feeb940b1d03b21e36b0e47e79769f095fe2ab855bd91e3a38756b7d75a9c4549"_hex,
        "000000000000000000000000a94f5374fce5edbc8e2a8697c15331677e6ebf0b"_hex},
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001b7af9e73057870458f03c143483bc5fcb6f39d01c9b26d28ed9f3fe23714f66283134a4ba8fafe11b351a720538398a5635e235c0b3258dce19942000731079ec"_hex,
        "0000000000000000000000009a04aede774152f135315670f562c19c5726df2c"_hex},
    // z >= Order
    {"fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141000000000000000000000000000000000000000000000000000000000000001b7af9e73057870458f03c143483bc5fcb6f39d01c9b26d28ed9f3fe23714f66283134a4ba8fafe11b351a720538398a5635e235c0b3258dce19942000731079ec"_hex,
        "000000000000000000000000b32CF3C8616537a28583FC00D29a3e8C9614cD61"_hex},
    {"6b8d2c81b11b2d699528dde488dbdf2f94293d0d33c32e347f255fa4a6c1f0a9000000000000000000000000000000000000000000000000000000000000001b79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f817986b8d2c81b11b2d699528dde488dbdf2f94293d0d33c32e347f255fa4a6c1f0a9"_hex,
        {}},
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001c0000000000000000000000000000000000000000000000000000000000000000eeb940b1d03b21e36b0e47e79769f095fe2ab855bd91e3a38756b7d75a9c4549"_hex,
        {}},
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001c73b1693892219d736caba55bdb67216e485557ea6b6af75f37096c9aa6a5a75f0000000000000000000000000000000000000000000000000000000000000000"_hex,
        {}},
    // r >= Order
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001cfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141eeb940b1d03b21e36b0e47e79769f095fe2ab855bd91e3a38756b7d75a9c4549"_hex,
        {}},
    // s >= Order
    {"18c547e4f7b0f325ad1e56f57e26c745b09a3e503d86e00e5255ff7f715d3d1c000000000000000000000000000000000000000000000000000000000000001c73b1693892219d736caba55bdb67216e485557ea6b6af75f37096c9aa6a5a75ffffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"_hex,
        {}},
    // u1 == u2 && R == G
    {"c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470 000000000000000000000000000000000000000000000000000000000000001b 79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798 3a2db9fe7908dcc36d81824d2338fc3dd5ae2692e4c6790043d7868872b09cd1"_hex,
        "0000000000000000000000002e4db28b1f03ec8acfc2865e0c08308730e7ddf2"_hex},
    // u1 == -u2 && R == -G
    {"c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470 000000000000000000000000000000000000000000000000000000000000001c 79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798 c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"_hex,
        "0000000000000000000000002e4db28b1f03ec8acfc2865e0c08308730e7ddf2"_hex},
    // 13u1 == u2 && R == -13G
    {"c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470 000000000000000000000000000000000000000000000000000000000000001b f28773c2d975288bc7d1d205c3748651b075fbc6610e58cddeeddf8f19405aa8 533e9827446324ac92450a05ef04622bc0081f8d5b394e4d7b514ed35c946ee9"_hex,
        {}},
    // 13u1 == u2 && R == 13G
    {"c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470 000000000000000000000000000000000000000000000000000000000000001c f28773c2d975288bc7d1d205c3748651b075fbc6610e58cddeeddf8f19405aa8 533e9827446324ac92450a05ef04622bc0081f8d5b394e4d7b514ed35c946ee9"_hex,
        "000000000000000000000000fc4b7e97f115ac81f9a6997254892b45e8159d46"_hex},
};

TEST(evmmax, ecrecovery)
{
    for (const auto& t : test_cases)
    {
        ASSERT_EQ(t.input.size(), 128);

        ethash::hash256 hash;
        std::memcpy(hash.bytes, t.input.data(), 32);
        const auto v{be::unsafe::load<uint256>(&t.input[32])};
        ASSERT_TRUE(v == 27 || v == 28);
        const auto r{be::unsafe::load<uint256>(&t.input[64])};
        const auto s{be::unsafe::load<uint256>(&t.input[96])};
        const bool parity = v == 28;

        const auto result = ecrecover(hash, r, s, parity);

        if (t.expected_output.empty())
        {
            EXPECT_FALSE(result.has_value());
        }
        else
        {
            ASSERT_EQ(t.expected_output.size(), 32);
            evmc::address e;
            memcpy(&e.bytes[0], &t.expected_output[12], 20);
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(*result, e);
        }
    }
}
