// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <test/utils/blockchaintest.hpp>
#include <test/utils/test_report.hpp>

namespace evmone::test
{
/// Execute the blockchain @p tests using the @p vm, recording what does not match into @p report.
void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm, TestReport& report);
}  // namespace evmone::test
