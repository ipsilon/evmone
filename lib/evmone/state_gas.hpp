// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <evmc/evmc.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace evmone
{
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

    /// A child returned reservoir that the parent's outstanding spill can
    /// absorb -- the condition the cross-frame refill acts on. Counted BEFORE
    /// the refill, so it measures inputs that reach the rule.
    ///
    /// Upstream moved this rule out of `StateGas::repay_spill` (deleted) into
    /// `absorb_child_state_gas` + `refill`, so the bump lives in
    /// instructions_calls.cpp now; the predicate there is the same
    /// `min(child.left, parent.spilled) > 0`.
    int64_t cross_frame_absorbable = 0;

    void reset() noexcept { *this = {}; }
};

/// The active probe. Read/reset by the harness around an execution.
inline thread_local StateGasProbe state_gas_probe;

/// A frame's state-gas counters with the charging rules on top (EIP-8037).
struct StateGas : evmc::StateGas
{
    /// Charges `cost`, first from `left`, then from `gas_left` (recorded in `spilled`).
    [[nodiscard]] bool charge(int64_t& gas_left, int64_t cost) noexcept
    {
        assert(cost >= 0);  // 0 charge happens in code deployment.
        assert(gas_left >= 0);
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

    /// Refund state-gas.
    ///
    /// Give the `cost` to `gas_left` (up to `spilled`) and `left` (whatever remains).
    void refill(int64_t& gas_left, int64_t cost) noexcept
    {
        assert(cost >= 0);  // 0 refill happens in absorb.
        const auto to_gas_left = std::min(cost, spilled);
        gas_left += to_gas_left;
        spilled -= to_gas_left;
        left += cost - to_gas_left;
    }
};
}  // namespace evmone
