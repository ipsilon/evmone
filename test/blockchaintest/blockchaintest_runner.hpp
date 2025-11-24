// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <test/utils/blockchaintest.hpp>

namespace evmone::test
{
void run_blockchain_tests(std::span<const BlockchainTest> tests, evmc::VM& vm);
}  // namespace evmone::test
