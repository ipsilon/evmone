// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/hex.hpp>
#include <gtest/gtest.h>
#include <test/state/precompiles_internal.hpp>
#include <array>

TEST(bn254, ecpairing_null_pairs)
{
    // Any number of null pairs should pass the pairing check.
    for (const auto n : {0, 1, 2, 3, 4, 5})
    {
        evmc::bytes input(192 * static_cast<size_t>(n), 0);
        std::array<uint8_t, 32> result{};
        const auto [status_code, output_size] = evmone::state::ecpairing_execute(
            input.data(), input.size(), result.data(), result.size());
        EXPECT_EQ(status_code, EVMC_SUCCESS);
        EXPECT_EQ(output_size, result.size());
        EXPECT_EQ(evmc::hex({result.data(), result.size()}),
            "0000000000000000000000000000000000000000000000000000000000000001");
    }
}
