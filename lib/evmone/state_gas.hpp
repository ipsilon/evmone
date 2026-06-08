// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

namespace evmone
{
/// EIP-8037: the two-dimensional state-gas pair carried by a single execution frame.
///
/// State gas is metered independently from the regular `gas_left`. `reservoir` is the
/// pool a frame draws state-gas charges from; `used` is the net amount it has consumed.
/// `used` may go NEGATIVE when a descendant's storage-clear credit exceeds this frame's
/// own charges. The names mirror the EIP's `state_gas_reservoir` / `execution_state_gas_used`.
struct StateGas
{
    int64_t reservoir = 0;  ///< Remaining state-gas reservoir (EIP: state_gas_reservoir).
    int64_t used = 0;       ///< Net state gas consumed (EIP: execution_state_gas_used).

    /// Charges `cost`, drawing from `reservoir` first and spilling any remainder into the
    /// caller's regular `gas_left`. Atomic: returns false without mutating any field when
    /// neither pool can cover the cost. `gas_left` is passed in because it belongs to the
    /// regular-gas dimension, not the state-gas pair.
    bool charge(int64_t& gas_left, int64_t cost) noexcept
    {
        if (cost <= 0)
            return true;
        if (reservoir >= cost)
        {
            reservoir -= cost;
            used += cost;
            return true;
        }
        const auto remainder = cost - reservoir;
        if (gas_left >= remainder)
        {
            used += cost;
            reservoir = 0;
            gas_left -= remainder;
            return true;
        }
        return false;
    }

    /// Refills `cost` back to the reservoir, undoing one charge's reservoir bookkeeping
    /// (EIP: state-gas "refilled back to the reservoir"). The inverse of charge().
    void refill(int64_t cost) noexcept
    {
        reservoir += cost;
        used -= cost;
    }

    /// Refills the whole `used` balance back to the reservoir on frame revert/halt,
    /// zeroing this frame's net state gas. Equivalent to refill(used).
    void refill_used() noexcept
    {
        reservoir += used;
        used = 0;
    }
};
}  // namespace evmone
