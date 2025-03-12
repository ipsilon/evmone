// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "precompiles_internal.hpp"

namespace evmone::state
{
ExecutionResult openssl_expmod_execute(
    const uint8_t* input, size_t input_size, uint8_t* output_buf, size_t max_output_size) noexcept;
}  // namespace evmone::state
