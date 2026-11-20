// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "modarith.hpp"

namespace evmone::crypto::safegcd
{
template <typename UintT>
UintT modinv_scaled_rt(const UintT& x, const UintT& scale, const UintT& mod) noexcept
{
    return modinv_scaled(x, scale, mod);
}

template intx::uint256 modinv_scaled(
    const intx::uint256& x, const intx::uint256& scale, const intx::uint256& mod) noexcept;
template intx::uint384 modinv_scaled(
    const intx::uint384& x, const intx::uint384& scale, const intx::uint384& mod) noexcept;
template intx::uint256 modinv_scaled_rt(
    const intx::uint256& x, const intx::uint256& scale, const intx::uint256& mod) noexcept;
template intx::uint384 modinv_scaled_rt(
    const intx::uint384& x, const intx::uint384& scale, const intx::uint384& mod) noexcept;
}  // namespace evmone::crypto::safegcd
