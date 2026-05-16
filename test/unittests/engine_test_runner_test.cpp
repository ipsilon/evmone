// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmc/evmc.hpp>
#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/engine_test.hpp>
#include <test/utils/utils.hpp>

using namespace evmone;
using namespace evmone::test;
using namespace testing;

namespace
{
EngineTest make_empty_engine_test()
{
    EngineTest t;
    t.name = "empty";
    t.network = "Osaka";
    t.rev = to_rev_schedule(t.network);
    t.genesis.hash =
        0x0000000000000000000000000000000000000000000000000000000000000003_bytes32;
    t.last_block_hash = t.genesis.hash;  // no payloads → head stays at genesis
    return t;
}
}  // namespace

TEST(engine_test_runner, empty_payloads_pass)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto result = run_engine_test(make_empty_engine_test(), vm);
    EXPECT_TRUE(result.passed) << result.error;
    EXPECT_EQ(result.error, "");
}
