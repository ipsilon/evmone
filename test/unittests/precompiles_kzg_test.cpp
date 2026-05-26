// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone_precompiles/kzg.hpp>
#include <evmone_precompiles/kzg_precomputed_lines.hpp>
#include <evmone_precompiles/sha256.hpp>
#include <gtest/gtest.h>
#include <intx/intx.hpp>
#include <span>

using namespace evmc::literals;
using namespace evmone::crypto;

namespace
{
constexpr auto G1_GENERATOR_X =
    0x17F1D3A73197D7942695638C4FA9AC0FC3688C4F9774B905A14E3A3F171BAC586C55E83FF97A1AEFFB3AF00ADB22C6BB_u384;
constexpr std::byte ZERO32[32]{};
constexpr std::byte POINT_AT_INFINITY[48]{std::byte{0xC0}};

auto versioned_hash(std::span<const std::byte> input) noexcept
{
    std::array<std::byte, 32> hash{};
    sha256(hash.data(), input.data(), input.size());
    hash[0] = VERSIONED_HASH_VERSION_KZG;
    return hash;
}
}  // namespace

TEST(kzg, verify_proof_hash_invalid)
{
    const auto r = kzg_verify_proof(ZERO32, ZERO32, ZERO32, POINT_AT_INFINITY, POINT_AT_INFINITY);
    EXPECT_FALSE(r);
}

TEST(kzg, verify_proof_zero)
{
    // Commit and prove polynomial f(x) = 0.
    std::byte z[32]{};
    z[13] = std::byte{17};  // can be any value because f(z) is always 0.
    const auto hash = versioned_hash(POINT_AT_INFINITY);
    const auto r = kzg_verify_proof(hash.data(), z, ZERO32, POINT_AT_INFINITY, POINT_AT_INFINITY);
    EXPECT_TRUE(r);
}

TEST(kzg, verify_g2_gen_lines)
{
    blst_fp6 expected[68];
    blst_precompute_lines(expected, blst_p2_affine_generator());
    const auto precomputed = g2_gen_lines();
    EXPECT_TRUE(std::memcmp(precomputed, expected, sizeof(expected)) == 0);
}

TEST(kzg, verify_kzg_setup_g2_1_lines)
{
    /// The point [s]₂ at index 1 of the G2 series of the Ethereum mainnet KZG
    /// trusted setup. Affine coordinates in Montgomery form. The compressed
    /// source (y-parity bit and Fp² x coordinate) is g2_monomial[1] at:
    /// https://github.com/ethereum/consensus-specs/blob/master/presets/mainnet/trusted_setups/trusted_setup_4096.json#L8200
    ///
    /// Not in the public header file because we don't want to expose blst types.
    constexpr blst_p2_affine KZG_SETUP_G2_1{
        {{{0x6120a2099b0379f9, 0xa2df815cb8210e4e, 0xcb57be5577bd3d4f, 0x62da0ea89a0c93f8,
              0x02e0ee16968e150d, 0x171f09aea833acd5},
            {0x11a3670749dfd455, 0x04991d7b3abffadc, 0x85446a8e14437f41, 0x27174e7b4e76e3f2,
                0x7bfa6dd397f60a20, 0x02fcc329ac07080f}}},
        {{{0xaa130838793b2317, 0xe236dd220f891637, 0x6502782925760980, 0xd05c25f60557ec89,
              0x6095767a44064474, 0x185693917080d405},
            {0x549f9e175b03dc0a, 0x32c0c95a77106cfe, 0x64a74eae5705d080, 0x53deeaf56659ed9e,
                0x09a1d368508afb93, 0x12cf3a4525b5e9bd}}}};

    blst_fp6 expected[68];
    blst_precompute_lines(expected, &KZG_SETUP_G2_1);
    const auto precomputed = kzg_setup_g2_1_lines();
    EXPECT_TRUE(std::memcmp(precomputed, expected, sizeof(expected)) == 0);
}

TEST(kzg, verify_proof_constant)
{
    // Commit and prove polynomial f(x) = 1.
    std::byte z[32]{};
    z[13] = std::byte{17};  // can be any value because f(z) is always 0.
    std::byte y[32]{};
    y[31] = std::byte{1};

    // Commitment for f(x) = 1 is [1]₁, i.e. the G1 generator point.
    std::byte c[48]{};
    intx::be::unsafe::store(reinterpret_cast<uint8_t*>(c), G1_GENERATOR_X);
    c[0] |= std::byte{0x80};  // flag of the point compressed form.

    const auto hash = versioned_hash(c);
    const auto r = kzg_verify_proof(hash.data(), z, y, c, POINT_AT_INFINITY);
    EXPECT_TRUE(r);
}

TEST(kzg, verify_proof_final_add_doubling)
{
    // Force the final G1 addition to be a doubling.
    //
    // Setup: π = O, y = 1, z = 0 ⇒ [z]π − [y]G1 = −G1.
    std::byte z[32]{};
    std::byte y[32]{};
    y[31] = std::byte{1};

    // C = −G1 (compressed): same X as G1 with the compressed flag (0x80)
    // and the Y-sign flag (0x20) set, since y(G1) is the lexicographically
    // smaller of the two square roots.
    std::byte c[48]{};
    intx::be::unsafe::store(reinterpret_cast<uint8_t*>(c), G1_GENERATOR_X);
    c[0] |= std::byte{0xA0};

    const auto hash = versioned_hash(c);
    EXPECT_FALSE(kzg_verify_proof(hash.data(), z, y, c, POINT_AT_INFINITY));
}
