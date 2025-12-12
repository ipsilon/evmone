// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "evmone_precompiles/bn254.hpp"
#include <gtest/gtest.h>
#include <test/utils/utils.hpp>

using namespace evmmax::bn254;
using namespace evmone::test;

TEST(evmmax, bn254_point_from_bytes_valid)
{
    static constexpr std::string_view TEST_CASES[]{
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000002",
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000",
    };

    for (const auto& input_hex : TEST_CASES)
    {
        static constexpr auto SIZE = sizeof(AffinePoint);
        auto input = from_hex(input_hex).value();
        ASSERT_EQ(input.size(), SIZE);

        const auto p =
            AffinePoint::from_bytes(std::span<const uint8_t, SIZE>{input.begin(), input.end()});
        ASSERT_TRUE(p.has_value());
        EXPECT_TRUE(validate(*p));
    }
}

TEST(evmmax, bn254_point_from_bytes_not_on_curve)
{
    static constexpr std::string_view TEST_CASES[]{
        "0000000000000000000000000000000000000000000000000000000000000001"
        "0000000000000000000000000000000000000000000000000000000000000000",
    };

    for (const auto& input_hex : TEST_CASES)
    {
        static constexpr auto SIZE = sizeof(AffinePoint);
        auto input = from_hex(input_hex).value();
        ASSERT_EQ(input.size(), SIZE);

        const auto p =
            AffinePoint::from_bytes(std::span<const uint8_t, SIZE>{input.begin(), input.end()});
        ASSERT_TRUE(p.has_value());
        EXPECT_FALSE(validate(*p));
    }
}

TEST(evmmax, bn254_point_from_bytes_fp_invalid)
{
    static constexpr std::string_view TEST_CASES[]{
        "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47"
        "0000000000000000000000000000000000000000000000000000000000000000",
        "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd48"
        "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd49",
        "0000000000000000000000000000000000000000000000000000000000000001"
        "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd49",
        "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd48"
        "0000000000000000000000000000000000000000000000000000000000000002",
    };

    for (const auto& input_hex : TEST_CASES)
    {
        static constexpr auto SIZE = sizeof(AffinePoint);
        auto input = from_hex(input_hex).value();
        ASSERT_EQ(input.size(), SIZE);

        const auto p =
            AffinePoint::from_bytes(std::span<const uint8_t, SIZE>{input.begin(), input.end()});
        EXPECT_FALSE(p.has_value());
    }
}
