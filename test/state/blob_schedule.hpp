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

/// Returns the blob schedule for the given EVM revision.
/// After Prague, the blob schedule is derived by BPO config.
constexpr BlobSchedule get_blob_schedule(evmc_revision rev)
{
    if (rev == EVMC_PRAGUE || rev == EVMC_EXPERIMENTAL)
        return {6, 9, 5007716};
    else if (rev > EVMC_PRAGUE)
        throw std::invalid_argument{
            "no hardcoded blob schedule for " + std::string{evmc::to_string(rev)}};
    else
        return {3, 6, 3338477};
}

BlobSchedule get_blob_schedule_by_bpo_fork(
    std::string_view network, const BlobScheduleMap& schedules, int64_t timestamp) noexcept;
BlobSchedule get_blob_schedule_by_bpo_fork(
    evmc_revision rev, const BlobScheduleMap& schedules) noexcept;
}  // namespace evmone::state
