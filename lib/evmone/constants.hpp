// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2021 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>

namespace evmone
{
/// The limit of the size of created contract
/// defined by [EIP-170](https://eips.ethereum.org/EIPS/eip-170)
constexpr auto MAX_CODE_SIZE = 0x6000;

/// The limit of the size of init codes for contract creation
/// defined by [EIP-3860](https://eips.ethereum.org/EIPS/eip-3860)
constexpr auto MAX_INITCODE_SIZE = 2 * MAX_CODE_SIZE;

/// EIP-7954: increased maximum contract code size for Amsterdam.
constexpr auto MAX_CODE_SIZE_AMSTERDAM = 0x8000;

/// EIP-7954: increased maximum init code size for Amsterdam.
constexpr auto MAX_INITCODE_SIZE_AMSTERDAM = 2 * MAX_CODE_SIZE_AMSTERDAM;

/// EIP-8037: Compute dynamic cost-per-state-byte from block gas limit.
/// Formula: raw = ceil((gas_limit * 2'628'000) / (2 * 100 * 1024^3))
///          shifted = raw + 9578
///          shift = max(bit_width(shifted) - 5, 0)
///          CPSB = max(((shifted >> shift) << shift) - 9578, 1)
///
/// Uses 128-bit intermediate to handle block gas limits up to INT64_MAX
/// (some tests use INT64_MAX as a sentinel) without overflow.
inline constexpr int64_t compute_cpsb(int64_t block_gas_limit) noexcept
{
    constexpr uint64_t TARGET_STATE_GROWTH_PER_YEAR = uint64_t{100} * 1024 * 1024 * 1024;
    constexpr uint64_t CPSB_SIGNIFICANT_BITS = 5;
    constexpr uint64_t CPSB_OFFSET = 9578;
    constexpr uint64_t BLOCKS_PER_YEAR = 2'628'000;

    const auto gl = static_cast<uint64_t>(block_gas_limit);
    const auto denominator = uint64_t{2} * TARGET_STATE_GROWTH_PER_YEAR;
    const auto numerator = static_cast<__uint128_t>(gl) * BLOCKS_PER_YEAR;
    const auto raw = static_cast<uint64_t>((numerator + denominator - 1) / denominator);
    const auto shifted = raw + CPSB_OFFSET;
    const auto bw = static_cast<uint64_t>(std::bit_width(shifted));
    const auto shift = (bw > CPSB_SIGNIFICANT_BITS) ? bw - CPSB_SIGNIFICANT_BITS : uint64_t{0};
    const auto rounded = ((shifted >> shift) << shift);
    const auto result = (rounded > CPSB_OFFSET) ? rounded - CPSB_OFFSET : uint64_t{1};
    return static_cast<int64_t>(std::max(result, uint64_t{1}));
}
}  // namespace evmone
