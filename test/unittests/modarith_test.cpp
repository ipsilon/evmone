// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone_precompiles/modarith.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <random>

using namespace intx;
using namespace evmone::crypto;

constexpr auto P23 = 23_u256;
constexpr auto BN254Mod = 0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_u256;
constexpr auto Secp256k1Mod =
    0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f_u256;
constexpr auto SECP256K1_ORDER =
    0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141_u256;
constexpr auto SECP256R1_MOD =
    0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff_u256;
constexpr auto SECP256R1_ORDER =
    0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551_u256;
constexpr auto M256 = 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_u256;
constexpr auto BLS12384Mod =
    0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab_u384;


namespace
{
template <typename UintT, const UintT& Mod>
struct ModA : ModArith<UintT>
{
    using uint = UintT;
    ModA() : ModArith<UintT>{Mod} {}
};

template <typename>
class modarith_test : public testing::Test
{};

template <typename Mod>
auto get_test_values(const Mod& m) noexcept
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

[[maybe_unused]] void constexpr_test()
{
    // Make sure ModArith works in constexpr.
    static constexpr ModArith m{BN254Mod};
    static_assert(m.mod() == BN254Mod);

    static constexpr auto a = m.to_mont(3);
    static constexpr auto b = m.to_mont(11);
    static_assert(m.add(a, b) == m.to_mont(14));
    static_assert(m.sub(a, b) == m.to_mont(BN254Mod - 8));
    static_assert(m.mul(a, b) == m.to_mont(33));
    static_assert(m.mul(m.inv(a), a) == m.to_mont(1));
    static_assert(m.inv(0) == 0);
}

template <typename Mod>
bool is_invertible(const typename Mod::uint& x) noexcept
{
    // The prime factors of M256 = 2²⁵⁶ - 1, the only modulus here with non-invertible values > 0.
    static constexpr std::array M256_FACTORS{3_u256, 5_u256, 17_u256, 257_u256, 641_u256,
        65537_u256, 274177_u256, 6700417_u256, 67280421310721_u256, 59649589127497217_u256,
        5704689200685129054721_u256};

    if constexpr (std::is_same_v<Mod, ModA<uint256, M256>>)
        return std::ranges::none_of(M256_FACTORS, [&](auto p) { return x % p == 0; });
    else
        return x != 0;
}
}  // namespace

using test_types = testing::Types<ModA<uint256, P23>, ModA<uint256, BN254Mod>,
    ModA<uint256, Secp256k1Mod>, ModA<uint256, SECP256K1_ORDER>, ModA<uint256, SECP256R1_MOD>,
    ModA<uint256, SECP256R1_ORDER>, ModA<uint256, M256>, ModA<uint384, BLS12384Mod>>;
TYPED_TEST_SUITE(modarith_test, test_types);

TYPED_TEST(modarith_test, to_from_mont)
{
    const typename TypeParam::uint v = 1;

    const TypeParam s;
    const auto x = s.to_mont(v);
    EXPECT_EQ(s.from_mont(x), v);
}

TYPED_TEST(modarith_test, to_from_mont_0)
{
    const TypeParam s;
    EXPECT_EQ(s.to_mont(0), 0);
    EXPECT_EQ(s.from_mont(0), 0);
}

TYPED_TEST(modarith_test, add)
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

TYPED_TEST(modarith_test, sub)
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

TYPED_TEST(modarith_test, mul)
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

TYPED_TEST(modarith_test, inv_random)
{
    using Uint = TypeParam::uint;
    const TypeParam m;
    std::mt19937_64 rng{2};  // NOLINT(cert-msc51-cpp)
    for (size_t i = 0; i < 1000; ++i)
    {
        Uint x;
        for (size_t j = 0; j < Uint::num_words; ++j)
            x[j] = rng();
        x %= m.mod();

        const auto xm_inv = m.inv(m.to_mont(x));
        if (!is_invertible<TypeParam>(x))
        {
            EXPECT_EQ(xm_inv, 0);
            continue;
        }
        EXPECT_LT(xm_inv, m.mod());
        EXPECT_EQ(m.from_mont(m.mul(m.to_mont(x), xm_inv)), 1);
    }
}

TYPED_TEST(modarith_test, modinv_structured)
{
    // modinv_scaled() of small values and values near powers of 2, whose runs of equal bits drive
    // the inversion to its limits; ModArith::inv() only sees Montgomery forms, which look random.
    // The scales are 1 and both ends of [0, mod); ModArith::inv() uses R² % mod.
    // For M256, 2⁶⁴ + 1 is not invertible but its gcd with M256 is 1 modulo 2⁶⁴.
    using Uint = TypeParam::uint;
    const TypeParam m;
    const auto check = [&](const Uint& x) {
        if (x >= m.mod())
            return;
        for (const auto& scale : {Uint{1}, Uint{0}, m.mod() - 1})
        {
            const auto x_inv = modinv_scaled(x, scale, m.mod());
            if (!is_invertible<TypeParam>(x))
                EXPECT_EQ(x_inv, 0) << hex(x);
            else
            {
                EXPECT_LT(x_inv, m.mod()) << hex(x);
                EXPECT_EQ(udivrem(umul(x, x_inv), m.mod()).rem, scale) << hex(x);
            }
        }
    };
    for (uint64_t j = 1; j < 1024; ++j)
        check(j);
    for (unsigned k = 10; k < Uint::num_bits; ++k)
    {
        const auto p = Uint{1} << k;
        for (uint64_t j = 0; j <= 8; ++j)
        {
            check(p + j);
            check(p - j);
        }
        check(m.mod() - p);
    }
}

TEST(modarith, modinv_many_divsteps)
{
    // The inputs needing the most divsteps of the safegcd algorithm that we found: 709 for 256 bits
    // and 1067 for 384 bits, while Theorem 11.2 of https://eprint.iacr.org/2019/266 bounds them by
    // 741 and 1110. They were found by running the divsteps backward from g = 0 and keeping the
    // predecessors of the smallest size.
    const auto check = []<typename Uint>(const Uint& x, const Uint& mod) {
        const auto x_inv = modinv_scaled(x, Uint{1}, mod);
        EXPECT_EQ(udivrem(umul(x, x_inv), mod).rem, 1);
    };
    check(0x8a285e7c34402ccfc61f06a750c782d02032bab6f5a035cfd32248aee0899657_u256,
        0x8c803e00ceb35ee2d2a74fa327c0799e1e16d40a072e10927a150123cd8d960d_u256);
    check(
        0xf0a6aa2ed2127c4fa6b7d42cf4a66f519e29f4e58476c245180fe73d2108d47311b621c48ec72f107fc121695ce9d2c5_u384,
        0xf85014810a9c3783b5da18cffc8c36bc78775b936c60a58c16f1f9ab7c19e735350b95889f789190c00a3579cebd35f9_u384);
}

TYPED_TEST(modarith_test, inv)
{
    const TypeParam m;
    for (const auto& x : get_test_values(m))
    {
        const auto xm = m.to_mont(x);
        const auto xm_inv = m.inv(xm);
        if (!is_invertible<TypeParam>(x))
        {
            EXPECT_EQ(xm_inv, 0);
            continue;
        }
        const auto pm = m.mul(xm, xm_inv);
        EXPECT_EQ(m.from_mont(pm), 1);
    }
}
