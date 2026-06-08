// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/state_gas.hpp>
#include <gtest/gtest.h>

using evmone::StateGas;

TEST(state_gas, default_is_zero)
{
    const StateGas sg;
    EXPECT_EQ(sg.reservoir, 0);
    EXPECT_EQ(sg.used, 0);
}

TEST(state_gas, charge_within_reservoir)
{
    StateGas sg{.reservoir = 100, .used = 0};
    int64_t gas_left = 1000;
    EXPECT_TRUE(sg.charge(gas_left, 30));
    EXPECT_EQ(sg.reservoir, 70);
    EXPECT_EQ(sg.used, 30);
    EXPECT_EQ(gas_left, 1000);  // Regular gas untouched while the reservoir covers the cost.
}

TEST(state_gas, charge_exact_reservoir)
{
    StateGas sg{.reservoir = 30, .used = 0};
    int64_t gas_left = 1000;
    EXPECT_TRUE(sg.charge(gas_left, 30));
    EXPECT_EQ(sg.reservoir, 0);
    EXPECT_EQ(sg.used, 30);
    EXPECT_EQ(gas_left, 1000);
}

TEST(state_gas, charge_spills_into_gas_left)
{
    StateGas sg{.reservoir = 20, .used = 0};
    int64_t gas_left = 1000;
    EXPECT_TRUE(sg.charge(gas_left, 50));
    EXPECT_EQ(sg.reservoir, 0);
    EXPECT_EQ(sg.used, 50);    // The full cost is recorded as used, including the spilled part.
    EXPECT_EQ(gas_left, 970);  // The 30 remainder is drawn from regular gas.
}

TEST(state_gas, charge_spill_exhausts_gas_left)
{
    StateGas sg{.reservoir = 20, .used = 0};
    int64_t gas_left = 30;  // Exactly the spill remainder.
    EXPECT_TRUE(sg.charge(gas_left, 50));
    EXPECT_EQ(sg.reservoir, 0);
    EXPECT_EQ(sg.used, 50);
    EXPECT_EQ(gas_left, 0);
}

TEST(state_gas, charge_atomic_failure_mutates_nothing)
{
    StateGas sg{.reservoir = 20, .used = 5};
    int64_t gas_left = 10;  // remainder 30 > gas_left, so neither pool can cover the cost.
    EXPECT_FALSE(sg.charge(gas_left, 50));
    EXPECT_EQ(sg.reservoir, 20);
    EXPECT_EQ(sg.used, 5);
    EXPECT_EQ(gas_left, 10);
}

TEST(state_gas, charge_nonpositive_cost_is_noop)
{
    StateGas sg{.reservoir = 20, .used = 5};
    int64_t gas_left = 10;
    EXPECT_TRUE(sg.charge(gas_left, 0));
    EXPECT_TRUE(sg.charge(gas_left, -100));
    EXPECT_EQ(sg.reservoir, 20);
    EXPECT_EQ(sg.used, 5);
    EXPECT_EQ(gas_left, 10);
}

TEST(state_gas, refill_reverses_a_reservoir_charge)
{
    StateGas sg{.reservoir = 100, .used = 0};
    int64_t gas_left = 1000;
    EXPECT_TRUE(sg.charge(gas_left, 30));
    sg.refill(30);
    EXPECT_EQ(sg.reservoir, 100);
    EXPECT_EQ(sg.used, 0);
    EXPECT_EQ(gas_left, 1000);
}

TEST(state_gas, refill_does_not_reclaim_spilled_gas_left)
{
    // The documented one-way spill: refill credits the whole cost back to the reservoir,
    // so the reservoir absorbs the spilled amount while regular gas stays reduced.
    StateGas sg{.reservoir = 20, .used = 0};
    int64_t gas_left = 1000;
    EXPECT_TRUE(sg.charge(gas_left, 50));  // 30 spills, gas_left -> 970.
    sg.refill(50);
    EXPECT_EQ(sg.reservoir, 50);  // Was 20: over-credited by the 30 that spilled.
    EXPECT_EQ(sg.used, 0);
    EXPECT_EQ(gas_left, 970);  // Spilled regular gas is not reclaimed.
}

TEST(state_gas, refill_used_returns_whole_balance)
{
    StateGas sg{.reservoir = 70, .used = 30};
    sg.refill_used();
    EXPECT_EQ(sg.reservoir, 100);
    EXPECT_EQ(sg.used, 0);
}

TEST(state_gas, refill_used_handles_negative_used)
{
    // A descendant's storage-clear credit can drive `used` negative before the fold.
    StateGas sg{.reservoir = 100, .used = -15};
    sg.refill_used();
    EXPECT_EQ(sg.reservoir, 85);
    EXPECT_EQ(sg.used, 0);
}
