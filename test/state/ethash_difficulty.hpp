// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.h>

namespace evmone::state
{
int64_t calculate_difficulty(int64_t parent_difficulty, bool parent_has_ommers,
        uint64_t parent_timestamp, uint64_t current_timestamp, int64_t block_number,
    evmc_revision rev) noexcept;
}  // namespace evmone::state
