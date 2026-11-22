// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2026 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

// Loops aligned to 64 bytes (-falign-loops=64): the loop position no longer depends on the
// code before it, so the 4 shifts must time the same.
#define JDP_MODE al
#define JDP_SUFFIX "+al"
#include "jumpdest_pointer_variants.inc"
