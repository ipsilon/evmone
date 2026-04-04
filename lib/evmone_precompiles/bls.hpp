#pragma once

#include <intx/intx.hpp>

using namespace intx::literals;

namespace evmone::crypto::bls
{
/// The BLS12-381 field prime number.
inline constexpr auto BLS_FIELD_MODULUS =
    0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab_u384;

/// Addition in BLS12-381 curve group.
///
/// Computes P ⊕ Q for two points in affine coordinates on the BLS12-381 curve,
[[nodiscard]] bool g1_add(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _x0[64],
    const uint8_t _y0[64], const uint8_t _x1[64], const uint8_t _y1[64]) noexcept;

/// Scalar multiplication in BLS12-381 curve G1 subgroup.
///
/// Computes [c]P for a point in affine coordinate on the BLS12-381 curve, performs subgroup check
/// according to spec https://eips.ethereum.org/EIPS/eip-2537#abi-for-g1-multiplication
[[nodiscard]] bool g1_mul(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _x[64],
    const uint8_t _y[64], const uint8_t _c[32]) noexcept;

/// Addition in BLS12-381 curve group over G2 extension field.
///
/// Computes P ⊕ Q for two points in affine coordinates on the BLS12-381 curve over G2 extension
/// field, performs subgroup checks for both points according to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-g2-addition
[[nodiscard]] bool g2_add(uint8_t _rx[128], uint8_t _ry[128], const uint8_t _x0[128],
    const uint8_t _y0[128], const uint8_t _x1[128], const uint8_t _y1[128]) noexcept;

/// Scalar multiplication in BLS12-381 curve group over G2 extension field
///
/// Computes [c]P for a point in affine coordinate on the BLS12-381 curve over G2 extension
/// field, performs subgroup check according to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-g2-multiplication
[[nodiscard]] bool g2_mul(uint8_t _rx[128], uint8_t _ry[128], const uint8_t _x[128],
    const uint8_t _y[128], const uint8_t _c[32]) noexcept;

/// Multi scalar multiplication in BLS12-381 curve G1 subgroup.
///
/// Computes ∑ⁿₖ₌₁cₖPₖ for points in affine coordinate on the BLS12-381 curve, performs
/// subgroup check according to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-g1-msm
[[nodiscard]] bool g1_msm(
    uint8_t _rx[64], uint8_t _ry[64], const uint8_t* _xycs, size_t size);

/// Multi scalar multiplication in BLS12-381 curve G2 subgroup.
///
/// Computes ∑ⁿₖ₌₁cₖPₖ for points in affine coordinate on the BLS12-381 curve  over G2 extension
/// field, performs subgroup check according to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-g2-msm
[[nodiscard]] bool g2_msm(
    uint8_t _rx[128], uint8_t _ry[128], const uint8_t* _xycs, size_t size);

/// Maps field element of Fp to curve point on BLS12-381 curve G1 subgroup.
///
/// Performs field Fp element check. Returns `false` if an element is not from the field.
/// According to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-mapping-fp-element-to-g1-point
[[nodiscard]] bool map_fp_to_g1(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _fp[64]) noexcept;

/// Maps field element of Fp2 to curve point on BLS12-381 curve G2 subgroup.
///
/// Performs field Fp2 element check. Returns `false` if an element is not from the field.
/// According to spec
/// https://eips.ethereum.org/EIPS/eip-2537#abi-for-mapping-fp2-element-to-g2-point
[[nodiscard]] bool map_fp2_to_g2(
    uint8_t _rx[128], uint8_t _ry[128], const uint8_t _fp[128]) noexcept;

/// Computes pairing for pairs of P and Q point from G1 and G2 accordingly.
///
/// Performs filed and groups check for both input points. Returns 'false' if any of requirement is
/// not met according to spec https://eips.ethereum.org/EIPS/eip-2537#abi-for-pairing-check
[[nodiscard]] bool pairing_check(uint8_t _r[32], const uint8_t* _pairs, size_t size) noexcept;

}  // namespace evmone::crypto::bls
