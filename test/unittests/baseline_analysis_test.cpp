// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/baseline.hpp>
#include <gtest/gtest.h>
#include <test/utils/bytecode.hpp>

using namespace evmone::test;

TEST(baseline_analysis, legacy)
{
    const auto code = push(1) + ret_top();
    const auto analysis = evmone::baseline::analyze(code);

    EXPECT_EQ(analysis.executable_code(), code);
    EXPECT_EQ(analysis.raw_code(), code);
    EXPECT_NE(analysis.raw_code().data(), code.data()) << "copy should be made";
}
