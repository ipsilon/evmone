// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execution_context.hpp"
#include <cassert>

namespace evmone
{
ExecutionState& ExecutionContext::create_execution_states(size_t depth) noexcept
{
    // The vector already has the capacity for all possible depths,
    // so reallocation never happens (therefore: noexcept).
    assert(depth < m_execution_states.capacity());
    m_execution_states.resize(depth + 1);
    return m_execution_states[depth];
}
}  // namespace evmone
