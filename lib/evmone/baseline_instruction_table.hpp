// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.h>
#include <array>

namespace evmone::baseline
{
using CostTable = std::array<int16_t, 256>;

const CostTable& get_baseline_cost_table(evmc_revision rev) noexcept;
}  // namespace evmone::baseline
