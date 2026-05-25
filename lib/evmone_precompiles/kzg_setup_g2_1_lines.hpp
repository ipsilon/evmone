// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <blst.h>

namespace evmone::crypto
{
/// Returns precomputed Miller-loop lines for KZG_SETUP_G2_1 ([s]₂ from the
/// Ethereum mainnet trusted setup), ready for blst_miller_loop_lines.
const blst_fp6 (&kzg_setup_g2_1_lines() noexcept)[68];

/// Recomputes the lines from KZG_SETUP_G2_1 via blst_precompute_lines and
/// returns true iff the result matches the baked-in table. Catches a stale
/// regeneration.
[[nodiscard]] bool verify_kzg_setup_g2_1_lines() noexcept;
}  // namespace evmone::crypto
