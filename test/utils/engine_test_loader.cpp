// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "engine_test.hpp"
#include "statetest.hpp"
#include "utils.hpp"

namespace evmone::test
{

std::vector<EngineTest> load_engine_tests(std::string_view /*json*/)
{
    throw UnsupportedTestFeature{"load_engine_tests not implemented"};
}

}  // namespace evmone::test
