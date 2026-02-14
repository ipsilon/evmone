// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <span>

namespace evmone::state
{
bool ecrecover_libsecp256k1(std::span<uint8_t, 64> pubkey, std::span<const uint8_t, 32> hash,
    std::span<const uint8_t, 64> sig_bytes, bool parity) noexcept;
}  // namespace evmone::state
