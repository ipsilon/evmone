// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>

namespace evmmax::ecc
{
/// Implements extension field over the base field or other extension fields.
/// It is a template struct which can be reused for different pairing implementations.
template <typename ConfigT>
struct ExtFieldElem
{
    using ValueT = typename ConfigT::ValueT;
    using Base = typename ConfigT::BaseFieldT;
    static constexpr auto DEGREE = ConfigT::DEGREE;
    using CoeffArrT = std::array<ValueT, DEGREE>;

    // TODO: Add operator[] for nicer access.
    CoeffArrT coeffs = {};

    constexpr ExtFieldElem() noexcept = default;

    /// Create an element from an array of coefficients.
    /// TODO: This constructor may be optimized to avoid copying the array.
    explicit constexpr ExtFieldElem(const CoeffArrT& cs) noexcept : coeffs{cs} {}

    constexpr ExtFieldElem conjugate() const noexcept
    {
        auto res = this->coeffs;
        for (size_t i = 1; i < DEGREE; i += 2)
            res[i] = -res[i];
        return ExtFieldElem(res);
    }

    static constexpr ExtFieldElem one() noexcept
    {
        ExtFieldElem res;
        res.coeffs[0] = ValueT::one();
        return res;
    }

    static constexpr ExtFieldElem zero() noexcept { return ExtFieldElem{}; }

    constexpr ExtFieldElem inv() const noexcept { return inverse(*this); }

    friend constexpr ExtFieldElem operator+(const ExtFieldElem& e1, const ExtFieldElem& e2) noexcept
    {
        auto res = e1.coeffs;
        for (size_t i = 0; i < DEGREE; ++i)
            res[i] = res[i] + e2.coeffs[i];
        return ExtFieldElem(res);
    }

    friend constexpr ExtFieldElem operator-(const ExtFieldElem& e1, const ExtFieldElem& e2) noexcept
    {
        auto res = e1.coeffs;
        for (size_t i = 0; i < DEGREE; ++i)
            res[i] = res[i] - e2.coeffs[i];
        return ExtFieldElem(res);
    }

    friend constexpr ExtFieldElem operator-(const ExtFieldElem& e) noexcept
    {
        CoeffArrT ret;
        for (size_t i = 0; i < DEGREE; ++i)
            ret[i] = -e.coeffs[i];
        return ExtFieldElem(ret);
    }

    friend constexpr ExtFieldElem operator*(const ExtFieldElem& e1, const ExtFieldElem& e2) noexcept
    {
        return multiply(e1, e2);
    }

    friend constexpr bool operator==(
        const ExtFieldElem& e1, const ExtFieldElem& e2) noexcept = default;

    friend constexpr ExtFieldElem operator*(const ExtFieldElem& e, const Base& s) noexcept
    {
        auto res = e;
        for (auto& c : res.coeffs)
            c = c * s;
        return res;
    }
};

}  // namespace evmmax::ecc
