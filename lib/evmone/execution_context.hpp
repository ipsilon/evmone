// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "execution_state.hpp"
#include <vector>

/// The EVMC execution context type. Defined by evmone as the base of evmone::ExecutionContext.
struct evmc_execution_context
{};

namespace evmone
{
/// The resources reused by executions: the execution states of all call depths.
class ExecutionContext : public evmc_execution_context
{
    std::vector<ExecutionState> m_execution_states;

    /// Creates the missing execution states up to and including the depth.
    /// Defined in execution_context.cpp: compiled into the interpreter's translation unit,
    /// even out of line, the vector resize and the ExecutionState constructor change GCC's
    /// inlining decisions in the dispatch loops.
    ExecutionState& create_execution_states(size_t depth) noexcept;

public:
    /// Reserves the capacity for all possible depths, so the vector never reallocates
    /// and the states of the outer frames stay in place while the nested calls execute.
    ExecutionContext() { m_execution_states.reserve(1025); }

    /// Returns the execution state of the call depth, creating it if needed.
    /// For the library's own use: create_execution_states() is not exported.
    [[nodiscard]] ExecutionState& get_execution_state(size_t depth) noexcept
    {
        // The ExecutionStates are lazily created because they pre-allocate EVM memory and stack.
        if (depth < m_execution_states.size()) [[likely]]
            return m_execution_states[depth];
        return create_execution_states(depth);
    }
};
}  // namespace evmone
