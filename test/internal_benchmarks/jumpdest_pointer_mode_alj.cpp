// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Loops aligned to 64 bytes, and no jump or macro-fused compare and jump crossing or ending on a
// 32-byte boundary (the Skylake JCC erratum mitigation of the assembler).
#define JDP_MODE alj
#define JDP_SUFFIX "+alj"
#include "jumpdest_pointer_variants.inc"
