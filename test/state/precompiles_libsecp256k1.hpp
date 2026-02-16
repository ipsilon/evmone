// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "precompiles_internal.hpp"
#include <span>

namespace evmone::state
{
/// Internal libsecp256k1-based implementation of the ECDSA public key recovery.
bool ecrecover_libsecp256k1(std::span<uint8_t, 64> pubkey, std::span<const uint8_t, 32> hash,
    std::span<const uint8_t, 64> sig_bytes, bool parity) noexcept;

/// Generic precompile API for libsecp256k1-based ecrecover implementation.
ExecutionResult ecrecover_execute_libsecp256k1(
    const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size) noexcept;
}  // namespace evmone::state
