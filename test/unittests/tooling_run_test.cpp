// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include <evmone/evmone.h>
#include <gmock/gmock.h>
#include <test/utils/bytecode.hpp>
#include <test/utils/run.hpp>
#include <sstream>

using namespace evmone;
using namespace evmone::test;
using namespace evmone::tooling;
using namespace testing;

TEST(tooling_run, execute)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto code = push(1);
    std::ostringstream out;
    const auto rc = run(vm, EVMC_OSAKA, 100, code, {}, false, false, out);
    EXPECT_EQ(rc, 0);
    EXPECT_THAT(out.str(), HasSubstr("Executing"));
    EXPECT_THAT(out.str(), HasSubstr("Osaka"));
    EXPECT_THAT(out.str(), HasSubstr("Result:   success"));
    EXPECT_THAT(out.str(), HasSubstr("Gas used: 3"));
}

TEST(tooling_run, create)
{
    evmc::VM vm{evmc_create_evmone()};
    const auto code = mstore(0, 0x5f) + ret(31, 1);
    std::ostringstream out;
    const auto rc = run(vm, EVMC_OSAKA, 100, code, {}, true, false, out);
    EXPECT_EQ(rc, 0);
    EXPECT_THAT(out.str(), HasSubstr("Creating"));
    EXPECT_THAT(out.str(), HasSubstr("Osaka"));
    EXPECT_THAT(out.str(), HasSubstr("Result:   success"));
    EXPECT_THAT(out.str(), HasSubstr("Gas used: 2"));
}
