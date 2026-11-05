// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2018 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.h>
#include <evmc/utils.h>

namespace evmone::advanced
{
struct AdvancedExecutionState;
struct AdvancedCodeAnalysis;

/// Execute the already analyzed code using the provided execution state.
EVMC_EXPORT evmc_result execute(
    AdvancedExecutionState& state, const AdvancedCodeAnalysis& analysis) noexcept;

/// EVMC-compatible execute() function.
evmc_result execute(evmc_vm* vm, evmc_execution_context* execution_context,
    const evmc_host_interface* host, evmc_host_context* ctx, evmc_revision rev,
    const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;
}  // namespace evmone::advanced
