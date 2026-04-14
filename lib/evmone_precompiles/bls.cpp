#include "bls.hpp"
#include <blst.h>
#include <memory>
#include <optional>
#include <vector>

namespace evmone::crypto::bls
{
namespace
{
/// Offset of the beginning of field element. First 16 bytes must be zero according to spec
/// https://eips.ethereum.org/EIPS/eip-2537#field-elements-encoding
constexpr auto FP_BYTES_OFFSET = 64 - 48;

/// Validates that integer encoded in big endian is valid element of BLS12-381 Fp field
[[nodiscard]] std::optional<blst_fp> validate_fp(const uint8_t _p[64]) noexcept
{
    if (intx::be::unsafe::load<intx::uint512>(_p) >= BLS_FIELD_MODULUS)
        return std::nullopt;

    blst_fp p;
    blst_fp_from_bendian(&p, &_p[FP_BYTES_OFFSET]);
    return p;
}

/// Validates p1 affine point. Checks that point coordinates are from the BLS12-381 field and
/// that the point is on curve. https://eips.ethereum.org/EIPS/eip-2537#abi-for-g1-addition
[[nodiscard]] std::optional<blst_p1_affine> validate_p1(
    const uint8_t _x[64], const uint8_t _y[64]) noexcept
{
    const auto x = validate_fp(_x);
    if (!x.has_value())
        return std::nullopt;
    const auto y = validate_fp(_y);
    if (!y.has_value())
        return std::nullopt;

    const blst_p1_affine p0_affine{*x, *y};
    if (!blst_p1_affine_on_curve(&p0_affine))
        return std::nullopt;

    return p0_affine;
}

/// Validates that integer encoded in big endian is valid element of BLS12-381 Fp2 extension field
[[nodiscard]] std::optional<blst_fp2> validate_fp2(const uint8_t _p[128]) noexcept
{
    const auto fp0 = validate_fp(_p);
    if (!fp0.has_value())
        return std::nullopt;
    const auto fp1 = validate_fp(&_p[64]);
    if (!fp1.has_value())
        return std::nullopt;

    return {{*fp0, *fp1}};
}

/// Validates p2 affine point. Checks that point coordinates are from the BLS12-381 field and
/// that the point is on curve. https://eips.ethereum.org/EIPS/eip-2537#abi-for-g2-addition
[[nodiscard]] std::optional<blst_p2_affine> validate_p2(
    const uint8_t _x[128], const uint8_t _y[128]) noexcept
{
    const auto x = validate_fp2(_x);
    if (!x.has_value())
        return std::nullopt;

    const auto y = validate_fp2(_y);
    if (!y.has_value())
        return std::nullopt;

    const blst_p2_affine p_affine{*x, *y};
    if (!blst_p2_affine_on_curve(&p_affine))
        return std::nullopt;

    return p_affine;
}

/// Stores fp in 64-bytes array with big endian encoding zero padded.
void store(uint8_t _rx[64], const blst_fp& _x) noexcept
{
    std::memset(_rx, 0, FP_BYTES_OFFSET);
    blst_bendian_from_fp(&_rx[FP_BYTES_OFFSET], &_x);
}

/// Stores fp2 in 128-bytes array with big endian encoding zero padded.
void store(uint8_t _rx[128], const blst_fp2& _x) noexcept
{
    store(_rx, _x.fp[0]);
    store(&_rx[64], _x.fp[1]);
}

}  // namespace

[[nodiscard]] bool g1_add(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _x0[64],
    const uint8_t _y0[64], const uint8_t _x1[64], const uint8_t _y1[64]) noexcept
{
    const auto p0_affine = validate_p1(_x0, _y0);
    const auto p1_affine = validate_p1(_x1, _y1);

    if (!p0_affine.has_value() || !p1_affine.has_value())
        return false;

    blst_p1 p0;
    blst_p1_from_affine(&p0, &*p0_affine);

    blst_p1 out;
    blst_p1_add_or_double_affine(&out, &p0, &*p1_affine);

    blst_p1_affine result;
    blst_p1_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool g1_mul(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _x[64],
    const uint8_t _y[64], const uint8_t _c[32]) noexcept
{
    blst_scalar scalar;
    blst_scalar_from_bendian(&scalar, _c);

    const auto p_affine = validate_p1(_x, _y);
    if (!p_affine.has_value())
        return false;

    blst_p1 p;
    blst_p1_from_affine(&p, &*p_affine);

    if (!blst_p1_in_g1(&p))
        return false;

    blst_p1 out;
    blst_p1_mult(&out, &p, scalar.b, 256);

    blst_p1_affine result;
    blst_p1_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool g2_add(uint8_t _rx[128], uint8_t _ry[128], const uint8_t _x0[128],
    const uint8_t _y0[128], const uint8_t _x1[128], const uint8_t _y1[128]) noexcept
{
    const auto p0_affine = validate_p2(_x0, _y0);
    const auto p1_affine = validate_p2(_x1, _y1);

    if (!p0_affine.has_value() || !p1_affine.has_value())
        return false;

    blst_p2 p0;
    blst_p2_from_affine(&p0, &*p0_affine);

    blst_p2 out;
    blst_p2_add_or_double_affine(&out, &p0, &*p1_affine);

    blst_p2_affine result;
    blst_p2_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool g2_mul(uint8_t _rx[128], uint8_t _ry[128], const uint8_t _x[128],
    const uint8_t _y[128], const uint8_t _c[32]) noexcept
{
    blst_scalar scalar;
    blst_scalar_from_bendian(&scalar, _c);

    const auto p_affine = validate_p2(_x, _y);
    if (!p_affine.has_value())
        return false;

    blst_p2 p;
    blst_p2_from_affine(&p, &*p_affine);

    if (!blst_p2_in_g2(&p))
        return false;

    blst_p2 out;
    blst_p2_mult(&out, &p, scalar.b, 256);

    blst_p2_affine result;
    blst_p2_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool g1_msm(uint8_t _rx[64], uint8_t _ry[64], const uint8_t* _xycs, size_t size)
{
    constexpr auto SINGLE_ENTRY_SIZE = (64 * 2 + 32);
    assert(size % SINGLE_ENTRY_SIZE == 0);
    const auto npoints = size / SINGLE_ENTRY_SIZE;

    std::vector<blst_p1_affine> p1_affines;
    std::vector<const blst_p1_affine*> p1_affine_ptrs;
    p1_affines.reserve(npoints);
    p1_affine_ptrs.reserve(npoints);

    std::vector<blst_scalar> scalars;
    std::vector<const uint8_t*> scalars_ptrs;
    scalars.reserve(npoints);
    scalars_ptrs.reserve(npoints);

    const auto end = _xycs + size;
    for (auto ptr = _xycs; ptr != end; ptr += SINGLE_ENTRY_SIZE)
    {
        const auto p_affine = validate_p1(ptr, &ptr[64]);
        if (!p_affine.has_value())
            return false;

        if (!blst_p1_affine_in_g1(&*p_affine))
            return false;

        // Point at infinity must be filtered out for BLST library.
        if (blst_p1_affine_is_inf(&*p_affine))
            continue;

        const auto& p = p1_affines.emplace_back(*p_affine);
        p1_affine_ptrs.emplace_back(&p);

        blst_scalar scalar;
        blst_scalar_from_bendian(&scalar, &ptr[128]);
        const auto& s = scalars.emplace_back(scalar);
        scalars_ptrs.emplace_back(s.b);
    }

    if (p1_affine_ptrs.empty())
    {
        std::memset(_rx, 0, 64);
        std::memset(_ry, 0, 64);
        return true;
    }

    const auto scratch_size =
        blst_p1s_mult_pippenger_scratch_sizeof(p1_affine_ptrs.size()) / sizeof(limb_t);
    const auto scratch_space = std::make_unique_for_overwrite<limb_t[]>(scratch_size);
    blst_p1 out;
    blst_p1s_mult_pippenger(&out, p1_affine_ptrs.data(), p1_affine_ptrs.size(), scalars_ptrs.data(),
        256, scratch_space.get());

    blst_p1_affine result;
    blst_p1_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool g2_msm(uint8_t _rx[128], uint8_t _ry[128], const uint8_t* _xycs, size_t size)
{
    constexpr auto SINGLE_ENTRY_SIZE = (128 * 2 + 32);
    assert(size % SINGLE_ENTRY_SIZE == 0);
    const auto npoints = size / SINGLE_ENTRY_SIZE;

    std::vector<blst_p2_affine> p2_affines;
    std::vector<const blst_p2_affine*> p2_affine_ptrs;
    p2_affines.reserve(npoints);
    p2_affine_ptrs.reserve(npoints);

    std::vector<blst_scalar> scalars;
    std::vector<const uint8_t*> scalars_ptrs;
    scalars.reserve(npoints);
    scalars_ptrs.reserve(npoints);

    const auto end = _xycs + size;
    for (auto ptr = _xycs; ptr != end; ptr += SINGLE_ENTRY_SIZE)
    {
        const auto p_affine = validate_p2(ptr, &ptr[128]);
        if (!p_affine.has_value())
            return false;

        if (!blst_p2_affine_in_g2(&*p_affine))
            return false;

        // Point at infinity must be filtered out for BLST library.
        if (blst_p2_affine_is_inf(&*p_affine))
            continue;

        const auto& p = p2_affines.emplace_back(*p_affine);
        p2_affine_ptrs.emplace_back(&p);

        blst_scalar scalar;
        blst_scalar_from_bendian(&scalar, &ptr[256]);
        const auto& s = scalars.emplace_back(scalar);
        scalars_ptrs.emplace_back(s.b);
    }

    if (p2_affine_ptrs.empty())
    {
        std::memset(_rx, 0, 128);
        std::memset(_ry, 0, 128);
        return true;
    }

    const auto scratch_size =
        blst_p2s_mult_pippenger_scratch_sizeof(p2_affine_ptrs.size()) / sizeof(limb_t);
    const auto scratch_space = std::make_unique_for_overwrite<limb_t[]>(scratch_size);
    blst_p2 out;
    blst_p2s_mult_pippenger(&out, p2_affine_ptrs.data(), p2_affine_ptrs.size(), scalars_ptrs.data(),
        256, scratch_space.get());

    blst_p2_affine result;
    blst_p2_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool map_fp_to_g1(uint8_t _rx[64], uint8_t _ry[64], const uint8_t _fp[64]) noexcept
{
    const auto fp = validate_fp(_fp);
    if (!fp.has_value())
        return false;

    blst_p1 out;
    blst_map_to_g1(&out, &*fp);

    blst_p1_affine result;
    blst_p1_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool map_fp2_to_g2(
    uint8_t _rx[128], uint8_t _ry[128], const uint8_t _fp2[128]) noexcept
{
    const auto fp2 = validate_fp2(_fp2);
    if (!fp2.has_value())
        return false;

    blst_p2 out;
    blst_map_to_g2(&out, &*fp2);

    blst_p2_affine result;
    blst_p2_to_affine(&result, &out);
    store(_rx, result.x);
    store(_ry, result.y);

    return true;
}

[[nodiscard]] bool pairing_check(uint8_t _r[32], const uint8_t* _pairs, size_t size) noexcept
{
    static constexpr auto FP_SIZE = 64;
    static constexpr auto FP2_SIZE = 2 * FP_SIZE;
    static constexpr auto P1_SIZE = 2 * FP_SIZE;
    static constexpr auto P2_SIZE = 2 * FP2_SIZE;
    static constexpr auto PAIR_SIZE = P1_SIZE + P2_SIZE;
    assert(size % PAIR_SIZE == 0);

    auto acc = *blst_fp12_one();
    const auto pairs_end = _pairs + size;
    for (auto ptr = _pairs; ptr != pairs_end; ptr += PAIR_SIZE)
    {
        const auto P_affine = validate_p1(ptr, &ptr[FP_SIZE]);
        if (!P_affine.has_value())
            return false;

        const auto Q_affine = validate_p2(&ptr[P1_SIZE], &ptr[P1_SIZE + FP2_SIZE]);
        if (!Q_affine.has_value())
            return false;

        if (!blst_p1_affine_in_g1(&*P_affine))
            return false;

        if (!blst_p2_affine_in_g2(&*Q_affine))
            return false;

        // Skip a pair containing any point at infinity.
        if (blst_p1_affine_is_inf(&*P_affine) || blst_p2_affine_is_inf(&*Q_affine))
            continue;

        blst_fp12 ml_res;
        blst_miller_loop(&ml_res, &*Q_affine, &*P_affine);
        blst_fp12_mul(&acc, &acc, &ml_res);
    }

    blst_final_exp(&acc, &acc);
    const auto result = blst_fp12_is_one(&acc);
    std::memset(_r, 0, 31);
    _r[31] = result ? 1 : 0;
    return true;
}

}  // namespace evmone::crypto::bls
