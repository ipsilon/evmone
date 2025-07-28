// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evmone_precompiles/bn254.hpp"
#include <gtest/gtest.h>

using namespace evmmax::bn254;
using namespace intx;


TEST(evmmax, bn254_pairing)
{
    const auto P1 = Point{
        0x1c76476f4def4bb94541d57ebba1193381ffa7aa76ada664dd31c16024c43f59_u256,
        0x3034dd2920f673e204fee2811c678745fc819b55d3e9d294e45c9b03a76aef41_u256,
    };
    // -P1:
    const auto nP1 = Point{P1.x, Curve::FIELD_PRIME - P1.y};
    // P1 * 17:
    const auto P1_17 = Point{
        0x22980b2e458ec77e258b19ca3a7b46181f63c6536307acae03eea236f6919eeb_u256,
        0x4eab993e2ba2cca2b08c216645e3fbcf80ae67515b2c49806c17b90c9d3cad3_u256,
    };

    const auto Q1 = ExtPoint{
        {
            0x04bf11ca01483bfa8b34b43561848d28905960114c8ac04049af4b6315a41678_u256,
            0x209dd15ebff5d46c4bd888e51a93cf99a7329636c63514396b4a452003a35bf7_u256,
        },
        {
            0x120a2a4cf30c1bf9845f20c6fe39e07ea2cce61f0c9bb048165fe5e4de877550_u256,
            0x2bb8324af6cfc93537a2ad1a445cfd0ca2a71acd7ac41fadbf933c2a51be344d_u256,
        },
    };
    // -Q1:
    const auto nQ1 =
        ExtPoint{Q1.x, {Curve::FIELD_PRIME - Q1.y.first, Curve::FIELD_PRIME - Q1.y.second}};
    // -Q1 * 16:
    const auto nQ1_16 = ExtPoint{
        {
            0x14191bd65f51663a1d4ad71d8480c3c3260d598aab6ed95681f773abade7fd7a_u256,
            0x299c79589dfb51fd6925fce3a7fc15c441fdafaa24f0d09b7c443befdddde4e5_u256,
        },
        {
            0x1d710ac19a995c6395f33be7f3dcd75e0632a006d196da6b4c9ba78708b6bb78_u256,
            0xcae1001513ae5ddf742aa6dc2f52457d9b14e17765dd74fc098ad06045d434e_u256,
        },
    };
    // -Q1 * 17:
    const auto nQ1_17 = ExtPoint{
        {
            0x11eeb08db4fe0df9d7617f11f5f8f488d643510f825f3730ffb038c84c9260fd_u256,
            0x12bf46039aa40a61762bf97b1bb028cebc6d42e46bbbe67f715eda54808b74c4_u256,
        },
        {
            0x42b65e62de1fd24534db81fd72e7ee832637948c1c466ccb08171e503f23e72_u256,
            0x197a5efb333448885788690df5af2211c1697dd8b7b1f8845b4e30a909d2b0f5_u256,
        },
    };

    {
        // p1*q1 - (-p1*q1) = 0?
        const std::vector<std::pair<Point, ExtPoint>> pairs{
            {P1, Q1},
            {nP1, Q1},
        };
        EXPECT_EQ(pairing_check(pairs), true);
    }

    {
        // p1*q1 - (p1*-q1) = 0?
        const std::vector<std::pair<Point, ExtPoint>> pairs{
            {P1, Q1},
            {P1, nQ1},
        };
        EXPECT_EQ(pairing_check(pairs), true);
    }

    {
        // p1*17*q1 - (p1*-q1*16) = 0?
        const std::vector<std::pair<Point, ExtPoint>> pairs{
            {P1_17, Q1},
            {P1, nQ1_16},
        };
        EXPECT_EQ(pairing_check(pairs), false);
    }

    {
        // p1*17 * q1 - (p1 * -q1*17) = 0?
        const std::vector<std::pair<Point, ExtPoint>> pairs{
            {P1_17, Q1},
            {P1, nQ1_17},
        };
        EXPECT_EQ(pairing_check(pairs), true);
    }

    // Empty input
    {
        EXPECT_EQ(pairing_check({}), true);
    }
}

TEST(evmmax, bn254_pairing_invalid_input)
{
    const std::vector<std::pair<Point, ExtPoint>> valid_input{{
        {
            0x22980b2e458ec77e258b19ca3a7b46181f63c6536307acae03eea236f6919eeb_u256,
            0x4eab993e2ba2cca2b08c216645e3fbcf80ae67515b2c49806c17b90c9d3cad3_u256,
        },
        {
            {
                0x04bf11ca01483bfa8b34b43561848d28905960114c8ac04049af4b6315a41678_u256,
                0x209dd15ebff5d46c4bd888e51a93cf99a7329636c63514396b4a452003a35bf7_u256,
            },
            {
                0x120a2a4cf30c1bf9845f20c6fe39e07ea2cce61f0c9bb048165fe5e4de877550_u256,
                0x2bb8324af6cfc93537a2ad1a445cfd0ca2a71acd7ac41fadbf933c2a51be344d_u256,
            },
        },
    }};

    EXPECT_EQ(pairing_check(valid_input), false);

    {
        // Coordinate not a field element
        auto input = valid_input;
        input[0].first.x = Curve::FIELD_PRIME;
        EXPECT_EQ(pairing_check(input), std::nullopt);
    }

    {
        // Coordinate not a field element
        auto input = valid_input;
        input[0].second.x.second = Curve::FIELD_PRIME;
        EXPECT_EQ(pairing_check(input), std::nullopt);
    }

    {
        // Point P (G1) not on curve
        auto input = valid_input;
        input[0].first.x += 1;
        EXPECT_EQ(pairing_check(input), std::nullopt);
    }

    {
        // Point Q (G2) not on curve
        auto input = valid_input;
        input[0].second.x.first += 1;
        EXPECT_EQ(pairing_check(input), std::nullopt);
    }

    {
        // Q not in proper group. Q id a member of small subgroup on twisted curve over Fq^2
        const ExtPoint Q{
            {
                0x13d841ba7ff3c6efd6870c3fea13a3ecab0423af5e4db9c5d28a6b46a05cd57b_u256,
                0x1a2b1eaa7b20faae36d26eff4db6e336c34434b66eded3cc5303d51ae353f478_u256,
            },
            {
                0x2d3e8808aa7a7fffa8f871f10df8d59c6dd725889c46e9136e01cb2465b20723_u256,
                0x1d5224817b8714531fc77e20b975178b1b3044f4b729fa3230db03dc0088ebdb_u256,
            },
        };

        auto input = valid_input;
        input[0].second = Q;
        EXPECT_EQ(pairing_check(input), std::nullopt);
    }
}

// Case taken from https://www.evm.codes/precompiled
TEST(evmmax_bn254, evm_codes_example)
{
    // Pair 1
    const auto p1 = Point{
        0x2cf44499d5d27bb186308b7af7af02ac5bc9eeb6a3d147c186b21fb1b76e18da_u256,
        0x2c0f001f52110ccfe69108924926e45f0b0c868df0e7bde1fe16d3242dc715f6_u256,
    };
    const auto q1 = ExtPoint{
        {
            0x22606845ff186793914e03e21df544c34ffe2f2f3504de8a79d9159eca2d98d9_u256,
            0x1fb19bb476f6b9e44e2a32234da8212f61cd63919354bc06aef31e3cfaff3ebc_u256,
        },
        {
            0x2fe02e47887507adf0ff1743cbac6ba291e66f59be6bd763950bb16041a0a85e_u256,
            0x2bd368e28381e8eccb5fa81fc26cf3f048eea9abfdd85d7ed3ab3698d63e4f90_u256,
        },
    };

    // Pair 2
    const auto p2 = Point{
        0x0000000000000000000000000000000000000000000000000000000000000001_u256,
        0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd45_u256,
    };
    const auto q2 = ExtPoint{
        {
            0x091058a3141822985733cbdddfed0fd8d6c104e9e9eff40bf5abfef9ab163bc7_u256,
            0x1971ff0471b09fa93caaf13cbf443c1aede09cc4328f5a62aad45f40ec133eb4_u256,
        },
        {
            0x23a8eb0b0996252cb548a4487da97b02422ebc0e834613f954de6c7e0afdc1fc_u256,
            0x2a23af9a5ce2ba2796c1f4e453a370eb0af8c212d9dc9acd8fc02c2e907baea2_u256,
        },
    };

    const std::vector<std::pair<Point, ExtPoint>> pairs = {{p1, q1}, {p2, q2}};
    const auto result = pairing_check(pairs);

    ASSERT_TRUE(result.has_value()) << "Pairing check should return a value.";
    EXPECT_EQ(result.value(), true) << "Pairing result should be true, matching Solidity.";
}

TEST(evmmax_bn254, evm_codes_example_changed_order)
{
    // Pair 1
    const auto p1 = Point{
        0x2cf44499d5d27bb186308b7af7af02ac5bc9eeb6a3d147c186b21fb1b76e18da_u256,
        0x2c0f001f52110ccfe69108924926e45f0b0c868df0e7bde1fe16d3242dc715f6_u256,
    };
    const auto q1 = ExtPoint{
        {
            0x1fb19bb476f6b9e44e2a32234da8212f61cd63919354bc06aef31e3cfaff3ebc_u256,
            0x22606845ff186793914e03e21df544c34ffe2f2f3504de8a79d9159eca2d98d9_u256,
        },
        {
            0x2bd368e28381e8eccb5fa81fc26cf3f048eea9abfdd85d7ed3ab3698d63e4f90_u256,
            0x2fe02e47887507adf0ff1743cbac6ba291e66f59be6bd763950bb16041a0a85e_u256,
        },
    };

    // Pair 2
    const auto p2 = Point{
        0x0000000000000000000000000000000000000000000000000000000000000001_u256,
        0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd45_u256,
    };
    const auto q2 = ExtPoint{
        {
            0x1971ff0471b09fa93caaf13cbf443c1aede09cc4328f5a62aad45f40ec133eb4_u256,
            0x091058a3141822985733cbdddfed0fd8d6c104e9e9eff40bf5abfef9ab163bc7_u256,
        },
        {
            0x2a23af9a5ce2ba2796c1f4e453a370eb0af8c212d9dc9acd8fc02c2e907baea2_u256,
            0x23a8eb0b0996252cb548a4487da97b02422ebc0e834613f954de6c7e0afdc1fc_u256,
        },
    };

    const std::vector<std::pair<Point, ExtPoint>> pairs = {{p1, q1}, {p2, q2}};
    const auto result = pairing_check(pairs);

    ASSERT_TRUE(!result.has_value())
        << "Pairing check should not return value. Modified points are not on twisted curve.";
}
