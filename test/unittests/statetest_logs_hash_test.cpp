// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/statetest.hpp>

using namespace evmone;
using namespace evmone::state;

TEST(statetest_logs_hash, empty_logs)
{
    EXPECT_EQ(test::logs_hash({}), EmptyListHash);
}

TEST(statetest_logs_hash, example1)
{
    const std::vector<Log> logs{
        Log{0x00_address, bytes{0xb0, 0xb1}, {}},
        Log{0xaa_address, {}, {0x01_bytes32, 0x02_bytes32}},
    };

    EXPECT_EQ(test::logs_hash(logs),
        0xb27f856c430c0266d2925d442632401e63685677a4ea009f855dee23e74488aa_bytes32);
}
