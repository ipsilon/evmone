// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <blst.h>

namespace evmone::crypto
{
/// Returns precomputed Miller-loop lines for the BLS12-381 G2 generator [1]₂.
const blst_fp6* g2_gen_lines() noexcept;

/// Returns precomputed Miller-loop lines for KZG_SETUP_G2_1 ([s]₂ from the
/// Ethereum mainnet trusted setup).
const blst_fp6* kzg_setup_g2_1_lines() noexcept;
}  // namespace evmone::crypto
