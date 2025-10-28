// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test/experimental/jumpdest_analysis.hpp"
#include "test/utils/bytecode.hpp"
#include <evmone/baseline.hpp>
#include <gtest/gtest.h>

using namespace evmone;
using namespace evmone::exp::jda;
using namespace evmone::test;

namespace
{
constexpr auto CODE_PADDING_CHECK_SIZE = 100;

auto baseline_analyze(bytes_view code)
{
    return baseline::analyze(code);
}

/// The set of test cases for jumpdest analysis.
/// They come from inspecting various implementations and from issues found by fuzzers.
const bytecode bytecode_test_cases[]{
    {},
    push(0x5b),
    OP_JUMPDEST,
    2 * OP_JUMPDEST,
    push(0),
    push(0x5b) + OP_JUMPDEST,
    push(0x60) + OP_JUMPDEST,
    bytecode{"00"} + OP_JUMPDEST,
    bytecode{"80"} + OP_JUMPDEST,
    bytecode{"5f"} + OP_JUMPDEST,
    bytecode{"ff"} + OP_JUMPDEST,
    bytecode{OP_STOP} + OP_JUMPDEST,
    push(0x5b) + OP_STOP + push(0x5b),
    OP_JUMPDEST + 30 * bytecode{0x00} + OP_PUSH1,
    30 * bytecode{0x00} + OP_PUSH32 + OP_JUMPDEST,
    OP_STOP + push(0x5b) + 27 * bytecode{0x00} + push(0x5b),
    OP_STOP + 3 * OP_JUMPDEST,
    32 * OP_STOP + push("00000000000000005b000000000000005b") + OP_JUMPDEST,
    "5b14000000000000005badadad0000000000000000000000606060606060ff5b",
};
}  // namespace

/// Wrapper for jumpdest analysis implementations suitable for typed tests.
template <typename T, T Fn(bytes_view)>
struct I
{
    static constexpr auto analyze = Fn;
};

template <typename>
class jumpdest_analysis_test : public testing::Test
{};
using test_types = testing::Types<                //
    I<baseline::CodeAnalysis, baseline_analyze>,  //
    I<JumpdestBitset, speculate_push_data_size>   //
    >;
TYPED_TEST_SUITE(jumpdest_analysis_test, test_types);

TYPED_TEST(jumpdest_analysis_test, validate)
{
    // Compare a jumpdest analysis implementation against the reference implementation.
    for (size_t test_idx = 0; test_idx < std::size(bytecode_test_cases); ++test_idx)
    {
        for (const auto extended : {true, false})
        {
            auto code = bytecode_test_cases[test_idx];
            if (extended)  // Extend size to multiply of 32 to force bulk-based implementations.
                code.resize(code.size() / 32 * 32);
            const auto expected = reference(code);
            const auto analysis = TypeParam::analyze(code);

            for (size_t i = 0; i < code.size() + CODE_PADDING_CHECK_SIZE; ++i)
            {
                EXPECT_EQ(analysis.check_jumpdest(i), expected.check_jumpdest(i))
                    << "case " << test_idx << (extended ? " extended" : "") << " [" << i << "]";
            }
        }
    }
}
