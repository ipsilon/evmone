// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2024 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <string_view>

namespace evmone::test
{
/// Hex of the code of 10 popular mainnet contracts.
extern const std::array<std::string_view, 10> test_bytecodes;
}  // namespace evmone::test
