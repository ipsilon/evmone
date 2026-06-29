// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cstdint>

namespace evmone
{
/// EIP-8037: a frame's state gas as a (reservoir-left, spilled) pair, metered
/// independently from the regular `gas_left`.
///
/// `left` is the remaining reservoir a frame draws state-gas charges from;
/// `spilled` is the portion of those charges that had to draw from `gas_left`
/// because the reservoir was insufficient. `spilled` is tracked so refunds and
/// frame rollback restore the exact pools the charge drew from, in LIFO order.
///
/// The net state gas a frame (and its children) consumed is not stored — it is
/// derived from the frame's initial reservoir: `used = initial - left + spilled`
/// (see `used()`). This holds across nested calls because a child's initial
/// reservoir is the parent's `left` at call time.
struct StateGas
{
    int64_t left = 0;     ///< Remaining state-gas reservoir (EIP: state_gas_reservoir).
    int64_t spilled = 0;  ///< Consumed state gas that drew from `gas_left`.

    /// Charges `cost`, drawing from the reservoir first and spilling any remainder into the
    /// regular `gas_left`. Atomic: returns false without mutating any field when neither pool
    /// can cover the cost.
    [[nodiscard]] bool charge(int64_t& gas_left, int64_t cost) noexcept
    {
        if (cost <= 0)
            return true;
        if (left >= cost)
        {
            left -= cost;
            return true;
        }
        const auto spill = cost - left;
        if (gas_left < spill)
            return false;
        gas_left -= spill;
        spilled += spill;
        left = 0;
        return true;
    }

    /// Credits a `cost` refund in LIFO order: the pool charged last is refilled first —
    /// `gas_left` up to `spilled`, then the reservoir — so the refund restores the exact
    /// pools the matching charge drew from.
    void refill(int64_t& gas_left, int64_t cost) noexcept
    {
        const auto from_gas_left = std::min(cost, spilled);
        gas_left += from_gas_left;
        spilled -= from_gas_left;
        left += cost - from_gas_left;
    }

    /// The net state gas consumed by this frame and its children, given the frame's initial
    /// reservoir (the `state_gas` it was handed). Equals `initial - left + spilled`.
    [[nodiscard]] int64_t used(int64_t initial) const noexcept { return initial - left + spilled; }
};
}  // namespace evmone
