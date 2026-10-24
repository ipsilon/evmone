// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "execution_state.hpp"
#include <cassert>
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

public:
    /// Reserves the capacity for all possible depths, so the vector never reallocates
    /// and the states of the outer frames stay in place while the nested calls execute.
    ExecutionContext() { m_execution_states.reserve(1025); }

    [[nodiscard]] ExecutionState& get_execution_state(size_t depth) noexcept
    {
        // The ExecutionStates are lazily created because they pre-allocate EVM memory and stack.
        assert(depth < m_execution_states.capacity());
        if (m_execution_states.size() <= depth)
            m_execution_states.resize(depth + 1);
        return m_execution_states[depth];
    }
};
}  // namespace evmone
