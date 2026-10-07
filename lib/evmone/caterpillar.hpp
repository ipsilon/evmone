// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "baseline.hpp"

namespace evmone
{
class VM;

namespace caterpillar
{
/// Executes in Caterpillar interpreter on the given external and initialized state.
EVMC_EXPORT evmc_result execute(
    const VM&, int64_t gas, ExecutionState& state, const baseline::CodeAnalysis& analysis) noexcept;

evmc_result execute(evmc_vm* c_vm, const evmc_host_interface* host, evmc_host_context* ctx,
    evmc_revision rev, const evmc_message* msg, const uint8_t* code, size_t code_size) noexcept;

}  // namespace caterpillar
}  // namespace evmone
