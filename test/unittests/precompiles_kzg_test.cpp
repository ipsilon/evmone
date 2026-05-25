// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone_precompiles/kzg.hpp>
#include <evmone_precompiles/kzg_setup_g2_1_lines.hpp>
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

TEST(kzg, precomputed_lines_match_blst_precompute)
{
    // Pins the baked-in line table against fresh blst_precompute_lines output.
    EXPECT_TRUE(verify_kzg_setup_g2_1_lines());
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
