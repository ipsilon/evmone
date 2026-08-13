// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

/// The benchmark loop for a body processing BATCH_SIZE items per single execution.
///
/// This is the google/benchmark's State::KeepRunningBatch() loop, but CodSpeed instruments
/// the range-based `for (auto _ : state)` loop only. In the CodSpeed builds the loop body
/// is executed exactly once, so the batch size is irrelevant there.
#ifdef CODSPEED_ENABLED
#define EVMONE_BENCH_LOOP_BATCH(STATE, BATCH_SIZE) for ([[maybe_unused]] auto _ : (STATE))
#else
#define EVMONE_BENCH_LOOP_BATCH(STATE, BATCH_SIZE) while ((STATE).KeepRunningBatch(BATCH_SIZE))
#endif
