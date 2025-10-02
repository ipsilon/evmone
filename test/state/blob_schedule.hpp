// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2025 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <evmc/evmc.hpp>

namespace evmone::state
{
/// The cost of a single blob in gas units (EIP-4844).
constexpr auto GAS_PER_BLOB = 0x20000;  // 2**17

/// The maximum number of blobs that can be included in a transaction (EIP-7594).
constexpr auto MAX_TX_BLOB_COUNT = 6;

/// The blob schedule for an EVM revision (EIP-7840).
struct BlobSchedule
{
    uint16_t target = 0;
    uint16_t max = 0;
    uint32_t base_fee_update_fraction = 0;
};

using BlobScheduleMap = std::unordered_map<std::string, state::BlobSchedule>;

/// Returns the hardcoded blob params for the given EVM revision.
/// After Prague, the blob params must be derived from config.
BlobSchedule get_blob_schedule(evmc_revision rev);

/// Returns the blob params for the given EVM revision and a blob schedule.
BlobSchedule get_blob_schedule(
    evmc_revision rev, const BlobScheduleMap& schedules) noexcept;

/// Returns the blob params for given a description of a test network (e.g. transitioning
/// across two forks at some time), a blob schedule and the timestamp.
BlobSchedule get_blob_schedule(
    std::string_view network, const BlobScheduleMap& schedules, int64_t timestamp) noexcept;
}  // namespace evmone::state
