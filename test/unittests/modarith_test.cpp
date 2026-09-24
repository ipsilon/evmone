// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/modarith.hpp>
#include <gtest/gtest.h>
#include <array>

using namespace intx;
using namespace evmone::crypto;

constexpr auto P23 = 23_u256;
constexpr auto BN254Mod = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto Secp256k1Mod =
    0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;
constexpr auto M256 = 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256;
constexpr auto BLS12384Mod =
    0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab_u384;


template <typename>
class modarith_test : public testing::Test
{};

using test_types = testing::Types<MontgomeryArith<P23>, MontgomeryArith<BN254Mod>,
    MontgomeryArith<Secp256k1Mod>, MontgomeryArith<M256>, MontgomeryArith<BLS12384Mod>>;
TYPED_TEST_SUITE(modarith_test, test_types);

TYPED_TEST(modarith_test, to_from_internal)
{
    const typename TypeParam::uint_type v = 1;

    const auto x = TypeParam::to_internal(v);
    EXPECT_EQ(TypeParam::from_internal(x), v);
}

TYPED_TEST(modarith_test, to_from_internal_0)
{
    EXPECT_EQ(TypeParam::to_internal(0), 0);
    EXPECT_EQ(TypeParam::from_internal(0), 0);
}

template <typename Arith>
static auto get_test_values() noexcept
{
    using Uint = Arith::uint_type;
    static constexpr auto& MOD = Arith::MOD;
    return std::array{
        MOD - 1,
        MOD - 2,
        MOD / 2 + 1,
        MOD / 2,
        MOD / 2 - 1,
        Uint{2},
        Uint{1},
        Uint{0},
    };
}

[[maybe_unused]] static void constexpr_test()
{
    // Make sure MontgomeryArith works in constexpr.
    using M = MontgomeryArith<BN254Mod>;
    static_assert(M::MOD == BN254Mod);

    static constexpr auto a = M::to_internal(3);
    static constexpr auto b = M::to_internal(11);
    static_assert(M::add(a, b) == M::to_internal(14));
    static_assert(M::sub(a, b) == M::to_internal(BN254Mod - 8));
    static_assert(M::mul(a, b) == M::to_internal(33));
}

TYPED_TEST(modarith_test, add)
{
    using M = TypeParam;
    const auto values = get_test_values<M>();

    for (const auto& x : values)
    {
        const auto xm = M::to_internal(x);
        for (const auto& y : values)
        {
            const auto expected =
                udivrem(intx::uint<M::uint_type::num_bits + 64>{x} + y, M::MOD).rem;

            const auto ym = M::to_internal(y);
            const auto s1m = M::add(xm, ym);
            const auto s1 = M::from_internal(s1m);
            EXPECT_EQ(s1, expected);

            // Conversion to Montgomery form is not necessary for addition to work.
            const auto s2 = M::add(x, y);
            EXPECT_EQ(s2, expected);
        }
    }
}

TYPED_TEST(modarith_test, sub)
{
    using M = TypeParam;
    const auto values = get_test_values<M>();

    for (const auto& x : values)
    {
        const auto xm = M::to_internal(x);
        for (const auto& y : values)
        {
            const auto expected =
                udivrem(intx::uint<M::uint_type::num_bits + 64>{x} + M::MOD - y, M::MOD).rem;

            const auto ym = M::to_internal(y);
            const auto d1m = M::sub(xm, ym);
            const auto d1 = M::from_internal(d1m);
            EXPECT_EQ(d1, expected);

            // Conversion to Montgomery form is not necessary for subtraction to work.
            const auto d2 = M::sub(x, y);
            EXPECT_EQ(d2, expected);
        }
    }
}

TYPED_TEST(modarith_test, mul)
{
    using M = TypeParam;
    const auto values = get_test_values<M>();

    for (const auto& x : values)
    {
        const auto xm = M::to_internal(x);
        for (const auto& y : values)
        {
            const auto expected = udivrem(umul(x, y), M::MOD).rem;

            const auto ym = M::to_internal(y);
            const auto pm = M::mul(xm, ym);
            const auto p = M::from_internal(pm);
            EXPECT_EQ(p, expected);
        }
    }
}

TYPED_TEST(modarith_test, inv)
{
    using M = TypeParam;
    for (const auto& x : get_test_values<M>())
    {
        const auto xm = M::to_internal(x);
        const auto xm_inv = M::inv(xm);
        if (xm_inv == 0)  // not invertible
        {
            if (M::MOD != M256)  // mod is prime
            {
                EXPECT_EQ(x, 0);
            }
            continue;
        }
        const auto pm = M::mul(xm, xm_inv);
        EXPECT_EQ(M::from_internal(pm), 1);
    }
}
