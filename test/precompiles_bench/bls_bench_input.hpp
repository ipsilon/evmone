// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019-2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "evmc/evmc.hpp"
#include "intx/intx.hpp"

namespace evmone::test
{
[[nodiscard]] std::array<evmc::bytes, 5> generate_g1_mul_input(
    const intx::uint256& scalar = std::numeric_limits<intx::uint256>::max()) noexcept;
[[nodiscard]] std::array<evmc::bytes, 5> generate_g2_mul_input(
    const intx::uint256& scalar = std::numeric_limits<intx::uint256>::max()) noexcept;
[[nodiscard]] evmc::bytes generate_g1_msm_input(
    size_t num, const intx::uint256& scalar = std::numeric_limits<intx::uint256>::max()) noexcept;
[[nodiscard]] evmc::bytes generate_g2_msm_input(
    size_t num, const intx::uint256& scalar = std::numeric_limits<intx::uint256>::max()) noexcept;
}  // namespace evmone::test
