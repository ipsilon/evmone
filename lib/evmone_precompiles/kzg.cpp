// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "kzg.hpp"
#include "kzg_setup_g2_1_lines.hpp"
#include <blst.h>
#include <algorithm>
#include <optional>
#include <span>

namespace evmone::crypto
{
namespace
{
/// The negation of the subgroup G1 generator -[1]₁ (affine coordinates in Montgomery form).
constexpr blst_p1_affine G1_GENERATOR_NEGATIVE{
    {0x5cb38790fd530c16, 0x7817fc679976fff5, 0x154f95c7143ba1c1, 0xf0ae6acdf3d0e747,
        0xedce6ecc21dbf440, 0x120177419e0bfb75},
    {0xff526c2af318883a, 0x92899ce4383b0270, 0x89d7738d9fa9d055, 0x12caf35ba344c12a,
        0x3cff1b76964b5317, 0x0e44d2ede9774430}};

/// Load and validate an element from the group order field.
std::optional<blst_scalar> validate_scalar(std::span<const std::byte, 32> b) noexcept
{
    blst_scalar v;
    blst_scalar_from_bendian(&v, reinterpret_cast<const uint8_t*>(b.data()));
    return blst_scalar_fr_check(&v) ? std::optional{v} : std::nullopt;
}

/// Uncompress and validate a point from G1 subgroup.
std::optional<blst_p1_affine> validate_G1(std::span<const std::byte, 48> b) noexcept
{
    blst_p1_affine r;
    if (blst_p1_uncompress(&r, reinterpret_cast<const uint8_t*>(b.data())) != BLST_SUCCESS)
        return std::nullopt;

    // Subgroup check is required by the spec but there are no test vectors
    // with points outside G1 which would satisfy the final pairings check.
    if (!blst_p1_affine_in_g1(&r))
        return std::nullopt;
    return r;
}

/// Add two points from E1 and convert the result to affine form.
/// The conversion to affine is very costly so use only if the affine of the result is needed.
blst_p1_affine add_or_double(const blst_p1_affine& p, const blst_p1& q) noexcept
{
    blst_p1 r;
    blst_p1_add_or_double_affine(&r, &q, &p);
    blst_p1_affine ra;
    blst_p1_to_affine(&ra, &r);
    return ra;
}

bool pairings_verify(const blst_p1_affine& a1, const blst_p1_affine& b1) noexcept
{
    blst_fp12 left;
    // Runs the Miller loop against the G2 generator (no precomputed lines).
    // TODO: Precompute lines for G2_GEN and use blst_miller_loop_lines() like the [s]₂ side.
    blst_aggregated_in_g1(&left, &a1);
    // Uses precomputed Miller loop lines for KZG_SETUP_G2_1 ([s]₂).
    blst_fp12 right;
    blst_miller_loop_lines(&right, kzg_setup_g2_1_lines(), &b1);
    return blst_fp12_finalverify(&left, &right);
}
}  // namespace

bool kzg_verify_proof(const std::byte versioned_hash[VERSIONED_HASH_SIZE], const std::byte z[32],
    const std::byte y[32], const std::byte commitment[48], const std::byte proof[48]) noexcept
{
    std::byte computed_versioned_hash[32];
    sha256(computed_versioned_hash, commitment, 48);
    computed_versioned_hash[0] = VERSIONED_HASH_VERSION_KZG;
    if (!std::ranges::equal(std::span{versioned_hash, 32}, computed_versioned_hash))
        return false;

    // Load and validate scalars z and y.
    // TODO(C++26): The span construction can be done as std::snap(z, std::c_<32>).
    const auto zz = validate_scalar(std::span<const std::byte, 32>{z, 32});
    if (!zz)
        return false;
    const auto yy = validate_scalar(std::span<const std::byte, 32>{y, 32});
    if (!yy)
        return false;

    // Uncompress and validate the points C (representing the polynomial commitment)
    // and Pi (representing the proof). They both are valid to be points at infinity
    // when they prove a commitment to a constant polynomial,
    // see https://hackmd.io/@kevaundray/kzg-is-zero-proof-sound
    const auto C = validate_G1(std::span<const std::byte, 48>{commitment, 48});
    if (!C)
        return false;
    const auto Pi = validate_G1(std::span<const std::byte, 48>{proof, 48});
    if (!Pi)
        return false;

    // The standard KZG verification equation
    //     e(C - [y]₁, [1]₂) =? e(π, [s - z]₂)
    // is rearranged via bilinearity into
    //     e(C + [z]π - [y]₁, [1]₂) =? e(π, [s]₂)
    // which eliminates the G2 multiplication and uses the 2-point MSM for G1.

    // Compute [z]π + [y](-[1]₁).
    const blst_p1_affine* const points[]{&*Pi, &G1_GENERATOR_NEGATIVE};
    const byte* const scalars[]{zz->b, yy->b};
    // For 2 points this actually doesn't use the Pippenger, and we can skip the scratch allocation.
    blst_p1 z_pi_minus_y_g1;
    blst_p1s_mult_pippenger(&z_pi_minus_y_g1, points, 2, scalars, BLS_MODULUS_BITS, nullptr);

    // Compute C + ([z]π - [y]₁). The addends may be the same / opposite points.
    const auto lsh_g1 = add_or_double(*C, z_pi_minus_y_g1);

    // e(C + [z]π - [y]₁, [1]₂) =? e(π, [s]₂)
    return pairings_verify(lsh_g1, *Pi);
}
}  // namespace evmone::crypto
