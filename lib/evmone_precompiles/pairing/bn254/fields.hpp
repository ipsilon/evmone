// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../../bn254.hpp"
#include "../../ecc.hpp"
#include "../field_template.hpp"

namespace evmmax::bn254
{
using namespace intx;

/// Specifies base field value type and modular arithmetic for bn254 curve.
struct BaseFieldConfig
{
    using ValueT = uint256;
    static constexpr auto& MOD_ARITH = Curve::Fp;
    static constexpr auto ONE = MOD_ARITH.to_mont(1);
};
using Fq = ecc::BaseFieldElem<BaseFieldConfig>;

// Extension fields implemented based on https://hackmd.io/@jpw/bn254#Field-extension-towers

/// Specifies Fq^2 extension field for bn254 curve. Base field extended with irreducible `u^2 + 1`
/// polynomial over the base field. `u` is the Fq^2 element.
struct Fq2Config
{
    using BaseFieldT = Fq;
    using ValueT = Fq;
    static constexpr auto DEGREE = 2;
};
using Fq2 = ecc::ExtFieldElem<Fq2Config>;

/// Specifies Fq^6 extension field for bn254 curve. Fq^2 field extended with irreducible
/// `v^3 - (9 + u)` polynomial over the Fq^2 field. `v` is the Fq^6 field element.
struct Fq6Config
{
    using BaseFieldT = Fq;
    using ValueT = Fq2;
    static constexpr uint8_t DEGREE = 3;
    static constexpr auto ksi = Fq2({Fq::from_int(9_u256), Fq::from_int(1_u256)});
    static constexpr auto _3_ksi_inv = Fq2({
        Fq::from_int(0x2b149d40ceb8aaae81be18991be06ac3b5b4c5e559dbefa33267e6dc24a138e5_u256),
        Fq::from_int(0x9713b03af0fed4cd2cafadeed8fdf4a74fa084e52d1852e4a2bd0685c315d2_u256),
    });
};
using Fq6 = ecc::ExtFieldElem<Fq6Config>;

/// Specifies Fq^12 extension field for bn254 curve. Fq^6 field extended with irreducible
/// `w^2 - v` polynomial over the Fq^2 field. `v` is the Fq^6 field element.
/// `w` is Fq^12 field element.
struct Fq12Config
{
    using BaseFieldT = Fq;
    using ValueT = Fq6;

    static constexpr uint8_t DEGREE = 2;
};
using Fq12 = ecc::ExtFieldElem<Fq12Config>;

/// Multiplies two Fq^2 field elements
constexpr Fq2 multiply(const Fq2& a, const Fq2& b)
{
    return Fq2({
        a.coeffs[0] * b.coeffs[0] - a.coeffs[1] * b.coeffs[1],
        a.coeffs[1] * b.coeffs[0] + a.coeffs[0] * b.coeffs[1],
    });
}

/// Multiplies two Fq^6 field elements
constexpr Fq6 multiply(const Fq6& a, const Fq6& b)
{
    const auto& a0 = a.coeffs[0];
    const auto& a1 = a.coeffs[1];
    const auto& a2 = a.coeffs[2];
    const auto& b0 = b.coeffs[0];
    const auto& b1 = b.coeffs[1];
    const auto& b2 = b.coeffs[2];

    const Fq2& ksi = Fq6Config::ksi;

    const auto t0 = a0 * b0;
    const auto t1 = a1 * b1;
    const auto t2 = a2 * b2;

    const auto c0 = ((a1 + a2) * (b1 + b2) - t1 - t2) * ksi + t0;
    const auto c1 = (a0 + a1) * (b0 + b1) - t0 - t1 + ksi * t2;
    const auto c2 = (a0 + a2) * (b0 + b2) - t0 - t2 + t1;

    return Fq6({c0, c1, c2});
}

/// Multiplies two Fq^12 field elements
constexpr Fq12 multiply(const Fq12& a, const Fq12& b)
{
    const auto& a0 = a.coeffs[0];
    const auto& a1 = a.coeffs[1];
    const auto& b0 = b.coeffs[0];
    const auto& b1 = b.coeffs[1];

    const auto t0 = a0 * b0;
    const auto t1 = a1 * b1;

    const Fq2& ksi = Fq6Config::ksi;

    const auto c0 = t0 + Fq6({ksi * t1.coeffs[2], t1.coeffs[0], t1.coeffs[1]});  // gamma is sparse.
    const auto c1 = (a0 + a1) * (b0 + b1) - t0 - t1;

    return Fq12({c0, c1});
}

/// Inverses the base field element
inline Fq inverse(const Fq& x)
{
    return Fq(BaseFieldConfig::MOD_ARITH.inv(x.value()));
}

/// Inverses the Fq^2 field element
inline Fq2 inverse(const Fq2& f)
{
    const auto& a0 = f.coeffs[0];
    const auto& a1 = f.coeffs[1];
    auto t0 = a0 * a0;
    auto t1 = a1 * a1;

    t0 = t0 + t1;
    t1 = t0.inv();

    const auto c0 = a0 * t1;
    const auto c1 = -(a1 * t1);

    return Fq2({c0, c1});
}

/// Inverses the Fq^6 field element
inline Fq6 inverse(const Fq6& f)
{
    const auto& a0 = f.coeffs[0];
    const auto& a1 = f.coeffs[1];
    const auto& a2 = f.coeffs[2];

    const Fq2& ksi = Fq6Config::ksi;

    const auto t0 = a0 * a0;
    const auto t1 = a1 * a1;
    const auto t2 = a2 * a2;

    const auto t3 = a0 * a1;
    const auto t4 = a0 * a2;
    const auto t5 = a2 * a1;

    const auto c0 = t0 - ksi * t5;
    const auto c1 = ksi * t2 - t3;
    const auto c2 = t1 - t4;

    const auto t = a0 * c0 + (a2 * c1 + a1 * c2) * ksi;
    const auto t6 = t.inv();

    return Fq6({c0 * t6, c1 * t6, c2 * t6});
}

/// Inverses the Fq^12 field element
inline Fq12 inverse(const Fq12& f)
{
    const auto& a0 = f.coeffs[0];
    const auto& a1 = f.coeffs[1];

    auto t0 = a0 * a0;
    auto t1 = a1 * a1;

    const Fq2& ksi = Fq6Config::ksi;

    t0 = t0 - Fq6({ksi * t1.coeffs[2], t1.coeffs[0], t1.coeffs[1]});  // gamma is sparse.
    t1 = t0.inv();

    const auto c0 = a0 * t1;
    const auto c1 = -(a1 * t1);

    return Fq12({c0, c1});
}
}  // namespace evmmax::bn254
