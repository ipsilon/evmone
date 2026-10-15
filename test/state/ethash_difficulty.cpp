// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2023 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "ethash_difficulty.hpp"
#include <algorithm>
#include <cassert>

namespace evmone::state
{
namespace
{
int64_t get_bomb_delay(evmc_revision rev) noexcept
{
    switch (rev)
    {
    default:
        return 0;
    case EVMC_BYZANTIUM:
        return 3'000'000;
    case EVMC_PETERSBURG:
    case EVMC_ISTANBUL:
        return 5'000'000;
    case EVMC_BERLIN:
        return 9'000'000;
    case EVMC_LONDON:
        return 9'700'000;
    }
}

int64_t calculate_difficulty_pre_byzantium(int64_t parent_difficulty, uint64_t parent_timestamp,
    uint64_t current_timestamp, int64_t block_number, evmc_revision rev)
{
    // According to https://eips.ethereum.org/EIPS/eip-2
    const auto period_count = block_number / 100'000;
    const auto offset = parent_difficulty / 2048;

    auto diff = parent_difficulty;

    // The timestamps are unsigned 64-bit values: compute their difference without overflow.
    const auto older_than_parent = current_timestamp < parent_timestamp;
    const auto timestamp_diff = older_than_parent ? parent_timestamp - current_timestamp :
                                                    current_timestamp - parent_timestamp;

    if (rev < EVMC_HOMESTEAD)
        diff += offset * (older_than_parent || timestamp_diff < 13 ? 1 : -1);
    else if (older_than_parent)
        diff += offset * (1 + static_cast<int64_t>(timestamp_diff / 10));
    else
    {
        // Cap the quotient before the signed conversion; the result is clamped to -99 anyway.
        const auto quotient = static_cast<int64_t>(std::min(timestamp_diff / 10, uint64_t{100}));
        diff += offset * std::max(1 - quotient, int64_t{-99});
    }

    if (period_count > 2)
        diff += 2 << (block_number / 100'000 - 3);
    else if (period_count == 2)
        diff += 1;

    return diff;
}

int64_t calculate_difficulty_since_byzantium(int64_t parent_difficulty, bool parent_has_ommers,
    uint64_t parent_timestamp, uint64_t current_timestamp, int64_t block_number,
    evmc_revision rev) noexcept
{
    const auto delay = get_bomb_delay(rev);
    const auto fake_block_number = std::max(int64_t{0}, block_number - delay);
    const auto p = (fake_block_number / 100'000) - 2;
    assert(p < 63);
    const auto epsilon = p < 0 ? 0 : int64_t{1} << p;
    const auto y = parent_has_ommers ? 2 : 1;

    assert(current_timestamp > parent_timestamp);
    const auto timestamp_diff = current_timestamp - parent_timestamp;
    // Cap the quotient before the signed conversion; sigma_2 is clamped to -99 anyway.
    const auto quotient = static_cast<int64_t>(std::min(timestamp_diff / 9, uint64_t{200}));
    const auto sigma_2 = std::max(y - quotient, int64_t{-99});
    const auto x = parent_difficulty / 2048;
    return parent_difficulty + x * sigma_2 + epsilon;
}
}  // namespace

int64_t calculate_difficulty(int64_t parent_difficulty, bool parent_has_ommers,
    uint64_t parent_timestamp, uint64_t current_timestamp, int64_t block_number,
    evmc_revision rev) noexcept
{
    // The calculation follows Ethereum Yellow Paper section 4.3.4. "Block Header Validity".
    static constexpr int64_t MIN_DIFFICULTY = 0x20000;

    if (rev >= EVMC_PARIS)
        return 0;  // No difficulty after the Merge.

    const auto difficulty =
        (rev < EVMC_BYZANTIUM) ?
            calculate_difficulty_pre_byzantium(
                parent_difficulty, parent_timestamp, current_timestamp, block_number, rev) :
            calculate_difficulty_since_byzantium(parent_difficulty, parent_has_ommers,
                parent_timestamp, current_timestamp, block_number, rev);

    return std::max(MIN_DIFFICULTY, difficulty);
}
}  // namespace evmone::state
