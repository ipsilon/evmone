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
/// A frame's state-gas counters with the charging rules on top (EIP-8037).
struct StateGas : evmc::StateGas
{
    /// Charges `cost`, first from `left`, then from `gas_left` (recorded in `spilled`).
    [[nodiscard]] bool charge(int64_t& gas_left, int64_t cost) noexcept
    {
        assert(cost >= 0);  // 0 charge happens in code deployment.
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
