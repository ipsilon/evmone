// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cstdint>

namespace evmone
{
/// A frame's state gas as a (reservoir-left, spilled) pair, metered independently
/// from the regular `gas_left` (EIP-8037).
///
/// `left` is the remaining reservoir a frame draws state-gas charges from;
/// `spilled` is the portion of those charges that had to draw from `gas_left`
/// because the reservoir was insufficient. `spilled` is tracked so refunds and
/// frame rollback restore the exact pools the charge drew from, in LIFO order.
///
/// The net state gas a frame (and its children) consumed is not stored — it is
/// derived from the frame's initial reservoir: `used = initial - left + spilled`.
/// This holds across nested calls because a child's initial reservoir is the
/// parent's `left` at call time.
/// EIP-8037 execution PROBE, for the differential fuzzer's coverage census.
///
/// The fuzzer needs to know whether an input actually REACHED the state-gas
/// paths, not merely whether it looked like it might. Every static proxy for
/// that has proved misleading: counting "the gas limit sits in a boundary
/// table" reported a healthy 0.35% while not one such input had a reservoir at
/// all, because they were all at a revision where EIP-8037 does not exist.
/// Only execution can answer it, so these count the two events that matter.
///
/// thread_local because the fuzzer runs backends on separate threads; zero cost
/// when unread, and nothing here feeds back into consensus behaviour.
struct StateGasProbe
{
    /// A charge that could not be covered by the reservoir and drew from
    /// `gas_left`. Non-zero means the input reached the dual-pool path.
    int64_t spills = 0;

    /// A child merged with state gas stranded in the reservoir that the
    /// parent's spill can absorb -- the condition repay_spill acts on. Counted
    /// BEFORE the repayment, so it measures inputs that reach the rule.
    int64_t cross_frame_absorbable = 0;

    void reset() noexcept { *this = {}; }
};

/// The active probe. Read/reset by the harness around an execution.
inline thread_local StateGasProbe state_gas_probe;

struct StateGas
{
    int64_t left = 0;     ///< Remaining state-gas reservoir (`state_gas_reservoir`).
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
        ++state_gas_probe.spills;
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

    /// Returns reservoir gas to `gas_left`, up to the spill still outstanding.
    ///
    /// A refill need not land in the frame whose charge spilled: a slot's original value is the
    /// value at transaction start, so a frame may clear a slot an earlier frame allocated. The
    /// credit then sits in the reservoir while the `gas_left` that funded the charge stays
    /// reduced. Applied when a child merges, this moves the credit up to the first frame with an
    /// outstanding spill. It undoes no state creation, so the net state gas used is unchanged.
    void repay_spill(int64_t& gas_left) noexcept
    {
        const auto amount = std::min(left, spilled);
        if (amount > 0)
            ++state_gas_probe.cross_frame_absorbable;
        gas_left += amount;
        left -= amount;
        spilled -= amount;
    }
};
}  // namespace evmone
