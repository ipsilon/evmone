// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/ecc.hpp>
#include <gtest/gtest.h>
#include <intx/intx.hpp>
#include <random>

using namespace evmmax::ecc;

namespace
{
template <typename UIntT>
UIntT evaluate(NAF<UIntT> naf)
{
    UIntT result = 0;
    UIntT base = 1;
    for (size_t i = 0; i < naf.width(); ++i)
    {
        const auto d = naf[i];
        const auto d_abs = static_cast<unsigned>(std::abs(d));
        const auto d_sign = d < 0;
        const auto r_abs = UIntT{d_abs};
        const auto r = d_sign ? -r_abs : r_abs;
        result += r * base;
        base <<= 1;
    }

    if (naf.width() == 0)
    {
        // NAF == 0 <=> result == 0.
        EXPECT_EQ(result, 0);
    }
    else
    {
        // The most significant digit must be non-zero.
        EXPECT_NE(naf[naf.width() - 1], 0);
    }
    return result;
}
}  // namespace

TEST(crypto_wnaf, example1)
{
    const auto naf = to_wnaf<3>(uint32_t{21});
    EXPECT_EQ(naf.width(), 4u);
    EXPECT_EQ(naf[0], -3);
    EXPECT_EQ(naf[1], 0);
    EXPECT_EQ(naf[2], 0);
    EXPECT_EQ(naf[3], 3);
    EXPECT_EQ(naf[4], 0);
    EXPECT_EQ(evaluate(naf), 21u);
}

TEST(crypto_wnaf, zero)
{
    const auto naf = to_wnaf<7>(uint64_t{0});
    EXPECT_EQ(naf.width(), 0);
    for (size_t i = 0; i <= 32; ++i)
        EXPECT_EQ(naf[i], 0);
    EXPECT_EQ(evaluate(naf), 0);
}

TEST(crypto_wnaf, max_width)
{
    const auto x = uint32_t{0xfffffffe};
    const auto naf = to_wnaf<4>(x);
    EXPECT_EQ(naf.width(), 33u);
    EXPECT_EQ(naf[0], 0);
    EXPECT_EQ(naf[1], -1);
    for (size_t i = 2; i <= 31; ++i)
        EXPECT_EQ(naf[i], 0);
    EXPECT_EQ(naf[32], 1);
    EXPECT_EQ(evaluate(naf), x);
}

TEST(crypto_wnaf, max_digit)
{
    const auto x = uint32_t{0xfffffcfe};
    const auto naf = to_wnaf<8>(x);
    EXPECT_EQ(naf.width(), 33u);
    EXPECT_EQ(naf[1], 127);
    EXPECT_EQ(evaluate(naf), x);
}

TEST(crypto_wnaf, min_digit)
{
    const auto x = uint32_t{0x102};
    const auto naf = to_wnaf<8>(x);
    EXPECT_EQ(naf.width(), 10u);
    EXPECT_EQ(naf[1], -127);
    EXPECT_EQ(evaluate(naf), x);
}

TEST(crypto_wnaf, uint256_fuzz)
{
    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist{};
    const intx::uint256 start{dist(rng), dist(rng), dist(rng), dist(rng)};

    for (size_t i = 0; i < 100; ++i)
    {
        const auto x = start + i;
        const auto naf2 = to_wnaf<2>(x);
        ASSERT_EQ(evaluate(naf2), x);
        const auto naf3 = to_wnaf<3>(x);
        ASSERT_EQ(evaluate(naf3), x);
        const auto naf4 = to_wnaf<4>(x);
        ASSERT_EQ(evaluate(naf4), x);
        const auto naf5 = to_wnaf<5>(x);
        ASSERT_EQ(evaluate(naf5), x);
        const auto naf8 = to_wnaf<8>(x);
        ASSERT_EQ(evaluate(naf8), x);
    }
}
