// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmmax/evmmax.hpp>
#include <gtest/gtest.h>
#include <array>
#include <vector>

using namespace intx;
using namespace evmmax;

constexpr auto P23 = 23_u256;
constexpr auto BN254Mod = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto Secp256k1Mod =
    0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;
constexpr auto M256 = 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256;
constexpr auto BLS12384Mod =
    0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab_u384;


// Only the secp256k1 field prime (2²⁵⁶-2³²-977) and 2²⁵⁶-1 are pseudo-Mersenne.
static_assert(pseudo_mersenne_c(Secp256k1Mod) == 0x1000003d1);
static_assert(pseudo_mersenne_c(M256) == 1);
static_assert(pseudo_mersenne_c(P23) == 0);
static_assert(pseudo_mersenne_c(BN254Mod) == 0);
static_assert(pseudo_mersenne_c(BLS12384Mod) == 0);

template <typename UintT, const UintT& Mod>
struct ModA : ModArith<UintT>
{
    using uint = UintT;
    ModA() : ModArith<UintT>{Mod} {}
};

template <typename>
class evmmax_test : public testing::Test
{};

using test_types = testing::Types<ModA<uint256, P23>, ModA<uint256, BN254Mod>,
    ModA<uint256, Secp256k1Mod>, ModA<uint256, M256>, ModA<uint384, BLS12384Mod>>;
TYPED_TEST_SUITE(evmmax_test, test_types);

TYPED_TEST(evmmax_test, to_from_mont)
{
    const typename TypeParam::uint v = 1;

    const TypeParam s;
    const auto x = s.to_mont(v);
    EXPECT_EQ(s.from_mont(x), v);
}

TYPED_TEST(evmmax_test, to_from_mont_0)
{
    const TypeParam s;
    EXPECT_EQ(s.to_mont(0), 0);
    EXPECT_EQ(s.from_mont(0), 0);
}

template <typename Mod>
static auto get_test_values(const Mod& m) noexcept
{
    using Uint = Mod::uint;
    return std::array{
        m.mod() - 1,
        m.mod() - 2,
        m.mod() / 2 + 1,
        m.mod() / 2,
        m.mod() / 2 - 1,
        Uint{2},
        Uint{1},
        Uint{0},
    };
}

[[maybe_unused]] static void constexpr_test()
{
    // Make sure ModArith works in constexpr.
    static constexpr ModArith m{BN254Mod};
    static_assert(m.mod() == BN254Mod);

    static constexpr auto a = m.to_mont(3);
    static constexpr auto b = m.to_mont(11);
    static_assert(m.add(a, b) == m.to_mont(14));
    static_assert(m.sub(a, b) == m.to_mont(BN254Mod - 8));
    static_assert(m.mul(a, b) == m.to_mont(33));
}

TYPED_TEST(evmmax_test, add)
{
    const TypeParam m;
    const auto values = get_test_values(m);

    for (const auto& x : values)
    {
        const auto xm = m.to_mont(x);
        for (const auto& y : values)
        {
            const auto expected =
                udivrem(intx::uint<TypeParam::uint::num_bits + 64>{x} + y, m.mod()).rem;

            const auto ym = m.to_mont(y);
            const auto s1m = m.add(xm, ym);
            const auto s1 = m.from_mont(s1m);
            EXPECT_EQ(s1, expected);

            // Conversion to Montgomery form is not necessary for addition to work.
            const auto s2 = m.add(x, y);
            EXPECT_EQ(s2, expected);
        }
    }
}

TYPED_TEST(evmmax_test, sub)
{
    const TypeParam m;
    const auto values = get_test_values(m);

    for (const auto& x : values)
    {
        const auto xm = m.to_mont(x);
        for (const auto& y : values)
        {
            const auto expected =
                udivrem(intx::uint<TypeParam::uint::num_bits + 64>{x} + m.mod() - y, m.mod()).rem;

            const auto ym = m.to_mont(y);
            const auto d1m = m.sub(xm, ym);
            const auto d1 = m.from_mont(d1m);
            EXPECT_EQ(d1, expected);

            // Conversion to Montgomery form is not necessary for subtraction to work.
            const auto d2 = m.sub(x, y);
            EXPECT_EQ(d2, expected);
        }
    }
}

TYPED_TEST(evmmax_test, mul)
{
    const TypeParam m;
    const auto values = get_test_values(m);

    for (const auto& x : values)
    {
        const auto xm = m.to_mont(x);
        for (const auto& y : values)
        {
            const auto expected = udivrem(umul(x, y), m.mod()).rem;

            const auto ym = m.to_mont(y);
            const auto pm = m.mul(xm, ym);
            const auto p = m.from_mont(pm);
            EXPECT_EQ(p, expected);
        }
    }
}

TYPED_TEST(evmmax_test, inv)
{
    const TypeParam m;
    for (const auto& x : get_test_values(m))
    {
        const auto xm = m.to_mont(x);
        const auto xm_inv = m.inv(xm);
        if (xm_inv == 0)  // not invertible
        {
            if (m.mod() != M256)  // mod is prime
            {
                EXPECT_EQ(x, 0);
            }
            continue;
        }
        const auto pm = m.mul(xm, xm_inv);
        EXPECT_EQ(m.from_mont(pm), 1);
    }
}

/// Reduces by generic division, as the reference for pseudo_mersenne_reduce().
template <const auto& Mod>
static auto reduce_ref(
    const intx::uint<2 * std::remove_cvref_t<decltype(Mod)>::num_bits>& p) noexcept
{
    using UintT = std::remove_cvref_t<decltype(Mod)>;
    return static_cast<UintT>(udivrem(p, decltype(p){Mod}).rem);
}

/// Checks pseudo_mersenne_reduce() against the reference for the whole input range, including
/// the two carry foldings that no product of two canonical operands can reach.
template <const auto& Mod>
static void test_reduce()
{
    using UintT = std::remove_cvref_t<decltype(Mod)>;
    using WideT = intx::uint<2 * UintT::num_bits>;
    static constexpr auto C = pseudo_mersenne_c(Mod);

    std::vector<WideT> inputs{0, 1, WideT{Mod} - 1, WideT{Mod} + 1, ~WideT{},
        umul(Mod - 1, Mod - 1), WideT{~UintT{}}, WideT{~UintT{}} - C, WideT{C}};

    // Reaches the final subtraction: a zero high half makes the folding an identity, so any
    // value in [Mod, 2ⁿ) needs the subtraction. Mod itself must reduce to 0.
    inputs.push_back(WideT{Mod});

    // Reaches the folding of the leftover word's overflow: solving l + h⋅C = 2ⁿ + (2ⁿ-1) leaves
    // the first folding at 2ⁿ-1 with a leftover word of 1, so adding 1⋅C overflows.
    const auto target = (WideT{1} << (UintT::num_bits + 1)) - 1;
    inputs.push_back(((target / C) << UintT::num_bits) | (target % C));

    for (const auto& p : inputs)
        EXPECT_EQ(pseudo_mersenne_reduce<Mod>(p), reduce_ref<Mod>(p)) << to_string(p, 16);
}

TEST(evmmax, pseudo_mersenne_reduce_secp256k1)
{
    test_reduce<Secp256k1Mod>();
}

TEST(evmmax, pseudo_mersenne_reduce_m256)
{
    test_reduce<M256>();
}
